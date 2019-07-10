# Minify with rollup

Write this in a file called _rollup.js_.

```js
import resolve from 'rollup-plugin-node-resolve';
import babel from 'rollup-plugin-babel';
import { uglify } from 'rollup-plugin-uglify';

export default {
    input: 'node_modules/lit-html/lit-html.js',
    plugins: [
        resolve({module: true}),
        babel({presets: ['@babel/preset-env']}),
        uglify()
    ],
    context: 'null',
    moduleContext: 'null',
    output: {
        file: 'lit-html.min.js',
        format: 'iife',
        name: 'lithtml'
    }
};
```

Run the following commands:

```bash
npm install lit-html rollup rollup-plugin-node-resolve rollup-plugin-babel rollup-plugin-uglify @babel/core @babel/preset-env
node_modules/.bin/rollup -c rollup.js
```
