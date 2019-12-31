# rollup-plugin-hypothetical [![npm][npm-image]][npm-url] [![Dependency Status][david-image]][david-url] [![Build Status][travis-image]][travis-url]
This allows [Rollup] modules to import hypothetical files passed in as options to the plugin.

## Installation
```bash
npm install --save-dev rollup-plugin-hypothetical
```

## Usage
```js
// rollup.config.js
import hypothetical from 'rollup-plugin-hypothetical';

export default {
  entry: './dir/a.js',
  plugins: [hypothetical({
    files: {
      './dir/a.js': `
        import foo from './b.js';
        foo();
      `,
      './dir/b.js': `
        import message from 'external';
        export default function foo() {
          console.log(message);
        }
      `,
      'external/': `
        export default "Hello, World!";
      `
    }
  })]
};
```

## Sourcemaps
To attach a sourcemap to a hypothetical file, simply pass in a `{ code, map }` object instead of a string. The sourcemap can have its own `sources`, `sourcesContent`, etc.

## Options
### options.files
An object whose keys are paths, either relative to the current working directory or absolute, and whose values are the code within the hypothetical files at those paths.

### options.filesMap
Rather than supplying a plain object to `options.files`, you may choose to supply a `Map` to `options.filesMap`. This will allow you to do things like naming your hypothetical files `__proto__`.

### options.allowFallthrough
Defaults to `false`. Set this to `true` to allow non-external imports to fall through. That way they can be handled either by Rollup's default behavior (reading from the filesystem) or by a plugin further down the chain if there is one.

### options.allowExternalFallthrough
Defaults to `true`. Set this to `false` to forbid external imports from falling through.

### options.allowRelativeExternalFallthrough
Defaults to `false`. Set this to `true` to allow relative imports from supplied external modules to fall through. For instance, suppose you have the following `options.files`:

```js
{
  './main.js': `
    import 'external/x.js';
  `,
  'external/x.js': `
    import './y.js';
  `
}
```

The supplied file `external/x.js` imports `external/y.js`, but `external/y.js` isn't supplied. This sort of thing is probably a mistake. If it isn't, set `options.allowRelativeExternalFallthrough` to `true` and **remember** to [include `external: ['external/y.js']` in the options you pass to `rollup.rollup`](https://rollupjs.org/#external-e-external-). If you forget that part, your build won't work, and weird things may happen instead!

### options.leaveIdsAlone
When this is set to `true`, the IDs in `import` statements won't be treated as paths and will instead be looked up directly in the `files` object. There will be no relative importing, path normalization, or restrictions on the contents of IDs.

### options.impliedExtensions
Set this to an array of file extensions to try appending to imports if an exact match isn't found. Defaults to `['.js', '/']`. If this is set to `false` or an empty array, file extensions and trailing slashes in imports will be treated as mandatory.

### options.cwd
When this is set to a directory name, relative file paths will be resolved relative to that directory rather than `process.cwd()`. When it's set to `false`, they will be resolved relative to an imaginary directory that cannot be expressed as an absolute path.


## License
MIT


[npm-url]:    https://npmjs.org/package/rollup-plugin-hypothetical
[david-url]:  https://david-dm.org/Permutatrix/rollup-plugin-hypothetical
[travis-url]: https://travis-ci.org/Permutatrix/rollup-plugin-hypothetical

[npm-image]:    https://img.shields.io/npm/v/rollup-plugin-hypothetical.svg
[david-image]:  https://img.shields.io/david/Permutatrix/rollup-plugin-hypothetical/master.svg
[travis-image]: https://img.shields.io/travis/Permutatrix/rollup-plugin-hypothetical/master.svg

[Rollup]: https://www.npmjs.com/package/rollup
