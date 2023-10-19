# Bundlers and Koffi

## Bundling

### Native modules

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
        win32_ia32/
            koffi.node
        win32_x64/
            koffi.node
        ...
MyApp.exe
```

### Indirect loader

*New in Koffi 2.6.2*

Some bundlers (such as vite) don't like when require is used with native modules.

In this case, you can use `require('koffi/indirect')` but you will need to make sure that the native Koffi modules are packaged properly.

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

### Node.js and esbuild

You can easily tell esbuild to copy the native files with the copy loader and things should just work. Use something like:

```sh
esbuild index.js --platform=node --bundle --loader:.node=copy --outdir=dist/
```

You can find a full [working example in the repository](https://github.com/Koromix/rygel/tree/master/src/koffi/examples/node-esbuild).
