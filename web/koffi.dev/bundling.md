# Packaging

## Native modules

*Improved in Koffi 2.5.8*

Koffi uses native modules to work. The NPM package contains binaries for various platforms and architectures, and the appropriate module is selected at runtime.

If you use a bundler, you must make sure that the {{ "`node_modules/koffi/build/" ~ stable ~ "`" }} directory is available in your bundled script.

Here is an example that would work:

```{eval-rst}
.. parsed-literal::
    koffi/
        |version|/
            win32_x64/
                koffi.node
            linux_x64/
                koffi.node
            ...
    MyBundle.js
```

When running in Electron, Koffi will also try to find the native module in `process.resourcesPath`. For an Electron app you could do something like this

```{eval-rst}
.. parsed-literal::
    locales/
    resources/
        koffi/
            |version|/
                darwin_arm64/
                    koffi.node
                darwin_x64/
                    koffi.node
    MyApp.exe
```

You must configure your bundler and/or your packager to make this work.

## Packaging examples

### Electron and electron-builder

Make sure electron-builder packages the native Koffi modules correctly with the following piece of configuration in `package.json`:

```json
  "build": {
    "extraResources": [
      { "from": "node_modules/koffi/build", "to": "koffi" }
    ]
  }
```

You can also go take a look at the [fully-working example in the repository](https://github.com/Koromix/rygel/tree/master/src/koffi/examples/electron).

### NW.js

Packagers such as nw-builder should work as-is. You can find a [working example in the repository](https://github.com/Koromix/rygel/tree/master/src/koffi/examples/nwjs).
