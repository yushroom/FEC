import * as std from 'std';

export function assert(actual, expected, message) {
    if (arguments.length == 1)
        expected = true;

    if (actual === expected)
        return;

    if (actual !== null && expected !== null
    &&  typeof actual == 'object' && typeof expected == 'object'
    &&  actual.toString() === expected.toString())
        return;

    throw Error("assertion failed: got |" + actual + "|" +
                ", expected |" + expected + "|" +
                (message ? " (" + message + ")" : ""));
};

export function print2(...values) {
    const a = values.map(x => { if (typeof(x)=='object') return JSON.stringify(x); return x});
    print(...a);
};

export function LoadFileAsJSON(path) {
    let f = std.open(path, 'r');
    const str = f.readAsString();
    f.close();
    return JSON.parse(str);
};