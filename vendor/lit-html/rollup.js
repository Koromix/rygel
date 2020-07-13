import { nodeResolve } from '@rollup/plugin-node-resolve';
import babel from 'rollup-plugin-babel';
import { uglify } from 'rollup-plugin-uglify';

export default {
    input: 'lit-html.js',
    plugins: [
        nodeResolve(),
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
