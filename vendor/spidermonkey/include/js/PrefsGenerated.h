/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_PrefsGenerated_h
#define js_PrefsGenerated_h

/* This file is generated by js/src/GeneratePrefs.py. Do not edit! */

#include "mozilla/Atomics.h"

#include <stdint.h>

#define JS_PREF_CLASS_FIELDS \
  static bool array_grouping_;\
  static bool arraybuffer_transfer_;\
  static bool destructuring_fuse_;\
  static mozilla::Atomic<bool, mozilla::Relaxed> experimental_shadow_realms_;\
  static bool experimental_weakrefs_expose_cleanupSome_;\
  static bool property_error_message_fix_;\
  static bool site_based_pretenuring_;\
  static mozilla::Atomic<uint32_t, mozilla::Relaxed> tests_uint32_pref_;\
  static bool use_emulates_undefined_fuse_;\
  static mozilla::Atomic<bool, mozilla::Relaxed> use_fdlibm_for_sin_cos_tan_;\
  static bool weakrefs_;\
  static bool well_formed_unicode_strings_;\


#define JS_PREF_CLASS_FIELDS_INIT \
  bool JS::Prefs::array_grouping_{true};\
  bool JS::Prefs::arraybuffer_transfer_{true};\
  bool JS::Prefs::destructuring_fuse_{true};\
  mozilla::Atomic<bool, mozilla::Relaxed> JS::Prefs::experimental_shadow_realms_{false};\
  bool JS::Prefs::experimental_weakrefs_expose_cleanupSome_{false};\
  bool JS::Prefs::property_error_message_fix_{false};\
  bool JS::Prefs::site_based_pretenuring_{true};\
  mozilla::Atomic<uint32_t, mozilla::Relaxed> JS::Prefs::tests_uint32_pref_{1};\
  bool JS::Prefs::use_emulates_undefined_fuse_{true};\
  mozilla::Atomic<bool, mozilla::Relaxed> JS::Prefs::use_fdlibm_for_sin_cos_tan_{false};\
  bool JS::Prefs::weakrefs_{true};\
  bool JS::Prefs::well_formed_unicode_strings_{true};\


#define FOR_EACH_JS_PREF(MACRO) \
  MACRO("array_grouping", array_grouping, bool, setAtStartup_array_grouping, true)\
  MACRO("arraybuffer_transfer", arraybuffer_transfer, bool, setAtStartup_arraybuffer_transfer, true)\
  MACRO("destructuring_fuse", destructuring_fuse, bool, setAtStartup_destructuring_fuse, true)\
  MACRO("experimental.shadow_realms", experimental_shadow_realms, bool, set_experimental_shadow_realms, false)\
  MACRO("experimental.weakrefs.expose_cleanupSome", experimental_weakrefs_expose_cleanupSome, bool, setAtStartup_experimental_weakrefs_expose_cleanupSome, true)\
  MACRO("property_error_message_fix", property_error_message_fix, bool, setAtStartup_property_error_message_fix, true)\
  MACRO("site_based_pretenuring", site_based_pretenuring, bool, setAtStartup_site_based_pretenuring, true)\
  MACRO("tests.uint32-pref", tests_uint32_pref, uint32_t, set_tests_uint32_pref, false)\
  MACRO("use_emulates_undefined_fuse", use_emulates_undefined_fuse, bool, setAtStartup_use_emulates_undefined_fuse, true)\
  MACRO("use_fdlibm_for_sin_cos_tan", use_fdlibm_for_sin_cos_tan, bool, set_use_fdlibm_for_sin_cos_tan, false)\
  MACRO("weakrefs", weakrefs, bool, setAtStartup_weakrefs, true)\
  MACRO("well_formed_unicode_strings", well_formed_unicode_strings, bool, setAtStartup_well_formed_unicode_strings, true)\


#define SET_JS_PREFS_FROM_BROWSER_PREFS \
  JS::Prefs::setAtStartup_array_grouping(mozilla::StaticPrefs::javascript_options_array_grouping());\
  JS::Prefs::setAtStartup_arraybuffer_transfer(mozilla::StaticPrefs::javascript_options_arraybuffer_transfer());\
  JS::Prefs::setAtStartup_destructuring_fuse(mozilla::StaticPrefs::javascript_options_destructuring_fuse());\
  JS::Prefs::set_experimental_shadow_realms(mozilla::StaticPrefs::javascript_options_experimental_shadow_realms());\
  JS::Prefs::setAtStartup_experimental_weakrefs_expose_cleanupSome(mozilla::StaticPrefs::javascript_options_experimental_weakrefs_expose_cleanupSome());\
  JS::Prefs::setAtStartup_property_error_message_fix(mozilla::StaticPrefs::javascript_options_property_error_message_fix());\
  JS::Prefs::setAtStartup_site_based_pretenuring(mozilla::StaticPrefs::javascript_options_site_based_pretenuring_DoNotUseDirectly());\
  JS::Prefs::set_tests_uint32_pref(mozilla::StaticPrefs::javascript_options_tests_uint32_pref());\
  JS::Prefs::setAtStartup_use_emulates_undefined_fuse(mozilla::StaticPrefs::javascript_options_use_emulates_undefined_fuse_DoNotUseDirectly());\
  JS::Prefs::set_use_fdlibm_for_sin_cos_tan(mozilla::StaticPrefs::javascript_options_use_fdlibm_for_sin_cos_tan());\
  JS::Prefs::setAtStartup_weakrefs(mozilla::StaticPrefs::javascript_options_weakrefs());\
  JS::Prefs::setAtStartup_well_formed_unicode_strings(mozilla::StaticPrefs::javascript_options_well_formed_unicode_strings());\


#define SET_NON_STARTUP_JS_PREFS_FROM_BROWSER_PREFS \
  JS::Prefs::set_experimental_shadow_realms(mozilla::StaticPrefs::javascript_options_experimental_shadow_realms());\
  JS::Prefs::set_tests_uint32_pref(mozilla::StaticPrefs::javascript_options_tests_uint32_pref());\
  JS::Prefs::set_use_fdlibm_for_sin_cos_tan(mozilla::StaticPrefs::javascript_options_use_fdlibm_for_sin_cos_tan());\




#endif // js_PrefsGenerated_h
