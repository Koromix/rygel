#include "napi.h"
#include <thread>
#include <cstdlib>
#include <condition_variable>
#include <mutex>

#if (NAPI_VERSION > 3)

using namespace Napi;

namespace {

struct TestData {

  TestData(Promise::Deferred&& deferred) : deferred(std::move(deferred)) {};
  
  // Native Promise returned to JavaScript
  Promise::Deferred deferred;

  // List of threads created for test. This list only ever accessed via main
  // thread.
  std::vector<std::thread> threads = {};

  ThreadSafeFunction tsfn = ThreadSafeFunction();

  // These variables are only accessed from the main thread.
  bool mainWantsRelease = false;
  size_t expected_calls = 0;
};

void FinalizerCallback(Napi::Env env, TestData* finalizeData){
  for (size_t i = 0; i < finalizeData->threads.size(); ++i) {
    finalizeData->threads[i].join();
  }
  finalizeData->deferred.Resolve(Boolean::New(env, true));
  delete finalizeData;
}

/**
 * See threadsafe_function_sum.js for descriptions of the tests in this file
 */

void entryWithTSFN(ThreadSafeFunction tsfn, int threadId) {
  std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 100 + 1));
  tsfn.BlockingCall( [=](Napi::Env env, Function callback) {
    callback.Call( { Number::New(env, static_cast<double>(threadId))});
  });
  tsfn.Release();
}

static Value TestWithTSFN(const CallbackInfo& info) {
  int threadCount = info[0].As<Number>().Int32Value();
  Function cb = info[1].As<Function>();

  // We pass the test data to the Finalizer for cleanup. The finalizer is
  // responsible for deleting this data as well.
  TestData *testData = new TestData(Promise::Deferred::New(info.Env()));

  ThreadSafeFunction tsfn = ThreadSafeFunction::New(
      info.Env(), cb, "Test", 0, threadCount,
      std::function<decltype(FinalizerCallback)>(FinalizerCallback), testData);

  for (int i = 0; i < threadCount; ++i) {
    // A copy of the ThreadSafeFunction will go to the thread entry point
    testData->threads.push_back( std::thread(entryWithTSFN, tsfn, i) );
  }

  return testData->deferred.Promise();
}

// Task instance created for each new std::thread
class DelayedTSFNTask {
public:
  // Each instance has its own tsfn
  ThreadSafeFunction tsfn;

  // Thread-safety
  std::mutex mtx;
  std::condition_variable cv;

  // Entry point for std::thread
  void entryDelayedTSFN(int threadId) {
    std::unique_lock<std::mutex> lk(mtx);
    cv.wait(lk);
    tsfn.BlockingCall([=](Napi::Env env, Function callback) {
      callback.Call({Number::New(env, static_cast<double>(threadId))});
    });
    tsfn.Release();
  };
};

struct TestDataDelayed {

  TestDataDelayed(Promise::Deferred &&deferred)
      : deferred(std::move(deferred)){};
  ~TestDataDelayed() { taskInsts.clear(); };
  // Native Promise returned to JavaScript
  Promise::Deferred deferred;

  // List of threads created for test. This list only ever accessed via main
  // thread.
  std::vector<std::thread> threads = {};

  // List of DelayedTSFNThread instances
  std::vector<std::unique_ptr<DelayedTSFNTask>> taskInsts = {};

  ThreadSafeFunction tsfn = ThreadSafeFunction();
};

void FinalizerCallbackDelayed(Napi::Env env, TestDataDelayed *finalizeData) {
  for (size_t i = 0; i < finalizeData->threads.size(); ++i) {
    finalizeData->threads[i].join();
  }
  finalizeData->deferred.Resolve(Boolean::New(env, true));
  delete finalizeData;
}

static Value TestDelayedTSFN(const CallbackInfo &info) {
  int threadCount = info[0].As<Number>().Int32Value();
  Function cb = info[1].As<Function>();

  TestDataDelayed *testData =
      new TestDataDelayed(Promise::Deferred::New(info.Env()));

  testData->tsfn =
      ThreadSafeFunction::New(info.Env(), cb, "Test", 0, threadCount,
                              std::function<decltype(FinalizerCallbackDelayed)>(
                                  FinalizerCallbackDelayed),
                              testData);

  for (int i = 0; i < threadCount; ++i) {
    testData->taskInsts.push_back(
        std::unique_ptr<DelayedTSFNTask>(new DelayedTSFNTask()));
    testData->threads.push_back(std::thread(&DelayedTSFNTask::entryDelayedTSFN,
                                            testData->taskInsts.back().get(),
                                            i));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 100 + 1));

  for (auto &task : testData->taskInsts) {
    std::lock_guard<std::mutex> lk(task->mtx);
    task->tsfn = testData->tsfn;
    task->cv.notify_all();
  }

  return testData->deferred.Promise();
}

void AcquireFinalizerCallback(Napi::Env env,
                              TestData* finalizeData,
                              TestData* context) {
  (void) context;
  for (size_t i = 0; i < finalizeData->threads.size(); ++i) {
    finalizeData->threads[i].join();
  }
  finalizeData->deferred.Resolve(Boolean::New(env, true));
  delete finalizeData;
}

void entryAcquire(ThreadSafeFunction tsfn, int threadId) {
  tsfn.Acquire();
  TestData* testData = tsfn.GetContext();
  std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 100 + 1));
  tsfn.BlockingCall( [=](Napi::Env env, Function callback) {
    // This lambda runs on the main thread so it's OK to access the variables
    // `expected_calls` and `mainWantsRelease`.
    testData->expected_calls--;
    if (testData->expected_calls == 0 && testData->mainWantsRelease)
      testData->tsfn.Release();
    callback.Call( { Number::New(env, static_cast<double>(threadId))});
  });
  tsfn.Release();
}

static Value CreateThread(const CallbackInfo& info) {
  TestData* testData = static_cast<TestData*>(info.Data());
  // Counting expected calls like this only works because on the JS side this
  // binding is called from a synchronous loop. This means the main loop has no
  // chance to run the tsfn JS callback before we've counted how many threads
  // the JS intends to create.
  testData->expected_calls++;
  ThreadSafeFunction tsfn = testData->tsfn;
  int threadId = testData->threads.size();
  // A copy of the ThreadSafeFunction will go to the thread entry point
  testData->threads.push_back( std::thread(entryAcquire, tsfn, threadId) );
  return Number::New(info.Env(), threadId);
}

static Value StopThreads(const CallbackInfo& info) {
  TestData* testData = static_cast<TestData*>(info.Data());
  testData->mainWantsRelease = true;
  return info.Env().Undefined();
}

static Value TestAcquire(const CallbackInfo& info) {
  Function cb = info[0].As<Function>();
  Napi::Env env = info.Env();

  // We pass the test data to the Finalizer for cleanup. The finalizer is
  // responsible for deleting this data as well.
  TestData *testData = new TestData(Promise::Deferred::New(info.Env()));

  testData->tsfn = ThreadSafeFunction::New(
    env, cb, "Test", 0, 1, testData,
    std::function<decltype(AcquireFinalizerCallback)>(AcquireFinalizerCallback),
    testData);

  Object result = Object::New(env);
  result["createThread"] = Function::New( env, CreateThread, "createThread", testData);
  result["stopThreads"] = Function::New( env, StopThreads, "stopThreads", testData);
  result["promise"] = testData->deferred.Promise();

  return result;
}
}

Object InitThreadSafeFunctionSum(Env env) {
  Object exports = Object::New(env);
  exports["testDelayedTSFN"] = Function::New(env, TestDelayedTSFN);
  exports["testWithTSFN"] = Function::New(env, TestWithTSFN);
  exports["testAcquire"] = Function::New(env, TestAcquire);
  return exports;
}

#endif
