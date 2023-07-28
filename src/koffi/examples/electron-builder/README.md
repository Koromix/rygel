This is a simple example shows:

- How to communicate with Koffi from a renderer window
- How to use electron-builder with an application that uses Koffi

One of the important piece for the packaging is in `package.json`, specifically this part:

```json
  "build": {
    "extraResources": [
      { "from": "node_modules/koffi/build", "to": "koffi" }
    ]
  }
```

This instructs electron-builder to copy the native koffi modules to a place where your application will find them.

Use the following commands to package the app for your system:

```sh
npm install
npm run dist
```
