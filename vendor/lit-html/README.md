# Update lit-html

Run the following commands:

```bash
npm install lit esbuild
node_modules/.bin/esbuild --bundle --platform=browser --global-name=lithtml --minify lit-html.js --outfile=lit-html.min.js
```

And that's all! You can remove node_modules and all the other Node.js crap.
