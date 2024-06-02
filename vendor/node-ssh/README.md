# Update lit-html

Run the following commands:

```bash
npm install --omit=optional node-ssh
npx esbuild --bundle --platform=node --loader:.node=empty --format=cjs node-ssh.js --outfile=node-ssh.bundle.js
```

And that's it! You can remove node_modules and all :)
