# Update Chart.js

Run the following commands:

```bash
npm install https://cdn.sheetjs.com/xlsx-0.20.1/xlsx-0.20.1.tgz
npx esbuild --bundle --platform=browser --format=esm xlsx.js --outfile=xlsx.bundle.js
```

And that's it! You can remove node_modules and all :)
