# Update Chart.js

Run the following commands:

```bash
npm install chart.js
npx esbuild --bundle --platform=browser --format=esm chart.js --outfile=chart.bundle.js
```

And that's it! You can remove node_modules and all :)
