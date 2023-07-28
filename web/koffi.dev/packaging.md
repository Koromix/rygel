# Packaging

## Bundling and native modules

*Simplified in Koffi 2.5.9*

Koffi uses native modules to work. The NPM package contains binaries for various platforms and architectures, and the appropriate module is selected at runtime.

In theory, the **packagers/bundlers should be able to find all native modules** because they are explictly listed in the Javascript file (as static strings) and package them somehow.

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

Packaging with electron-builder should work as-is.

Take a look at the full [working example in the repository](https://github.com/Koromix/rygel/tree/master/src/koffi/examples/electron-builder).

### Electron Forge

Packaging with Electron Force should work as-is, even when using webpack as configured initially when you run:

```sh
npm init electron-app@latest my-app -- --template=webpack
```

Take a look at the full [working example in the repository](https://github.com/Koromix/rygel/tree/master/src/koffi/examples/electron-forge).

### NW.js

Packagers such as nw-builder should work as-is.

You can find a full [working example in the repository](https://github.com/Koromix/rygel/tree/master/src/koffi/examples/nwjs).
