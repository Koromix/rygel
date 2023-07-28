# Packaging

## Bundling and native modules

*Simplified in Koffi 2.5.9*

Koffi uses native modules to work. The NPM package contains binaries for various platforms and architectures, and the appropriate module is selected at runtime.

In theory, the bundlers should be able to find recorgnize all native modules because they are explictly listed in the Javascript file, as static strings.

If that is not the case, you can manually arrange to copy the `node_modules/koffi/build/koffi` directory next to your bundled script.

Here is an example that would work:

```
    koffi/
        win32_x64/
            koffi.node
        linux_x64/
            koffi.node
        ...
    MyBundle.js
```

When running in Electron, Koffi will also try to find the native module in `process.resourcesPath`. For an Electron app you could do something like this

```
    locales/
    resources/
        koffi/
            darwin_arm64/
                koffi.node
            darwin_x64/
                koffi.node
    MyApp.exe
```

## Packaging examples

### Electron with electron-builder

Make sure electron-builder packages the native Koffi modules correctly with the following piece of configuration in `package.json`:

```json
  "build": {
    "extraResources": [
      { "from": "node_modules/koffi/build", "to": "koffi" }
    ]
  }
```

You can also go take a look at the full [working example in the repository](https://github.com/Koromix/rygel/tree/master/src/koffi/examples/electron-builder).

### Electron with electron-forge

Packaging with electron-forge should work as-is.

You can also go take a look at the full [working example in the repository](https://github.com/Koromix/rygel/tree/master/src/koffi/examples/electron-forge).

### NW.js

Packagers such as nw-builder should work as-is.

You can find a full [working example in the repository](https://github.com/Koromix/rygel/tree/master/src/koffi/examples/nwjs).
