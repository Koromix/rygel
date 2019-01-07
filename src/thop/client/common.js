// ------------------------------------------------------------------------
// Text
// ------------------------------------------------------------------------

function numberText(n)
{
    return n.toLocaleString('fr-FR');
}

function percentText(fraction)
{
    const parameters = {
        style: 'percent',
        minimumFractionDigits: 1,
        maximumFractionDigits: 1
    };
    return fraction.toLocaleString('fr-FR', parameters);
}

function priceText(price_cents, format_cents)
{
    if (format_cents === undefined)
        format_cents = true;

    if (price_cents !== undefined) {
        const parameters = {
            minimumFractionDigits: format_cents ? 2 : 0,
            maximumFractionDigits: format_cents ? 2 : 0
        };
        return (price_cents / 100.0).toLocaleString('fr-FR', parameters);
    } else {
        return '';
    }
}

function durationText(duration)
{
    if (duration !== null && duration !== undefined) {
        return duration.toString() + (duration >= 2 ? ' nuits' : ' nuit');
    } else {
        return '';
    }
}

function ageText(age)
{
    if (age !== null && age !== undefined) {
        return age.toString() + (age >= 2 ? ' ans' : ' an');
    } else {
        return '';
    }
}

// ------------------------------------------------------------------------
// Aggregation
// ------------------------------------------------------------------------

function Aggregator(template, func, by)
{
    let self = this;

    let list = [];
    let map = {};

    this.findPartial = function(values) {
        if (!Array.isArray(values))
            values = Array.prototype.slice.call(arguments, 0);

        let ptr = map;
        for (const value of values) {
            ptr = ptr[value];
            if (ptr === undefined)
                return null;
        }

        return ptr;
    };

    this.find = function(values) {
        let ptr = self.findPartial.apply(null, arguments);
        if (ptr && ptr.count === undefined)
            return null;

        return ptr;
    };

    function aggregateRec(row, row_ptrs, ptr, col_idx, key_idx)
    {
        for (let i = key_idx; i < by.length; i++) {
            let key = by[i];
            let value;
            if (typeof key === 'function') {
                let ret = key(row);
                key = ret[0];
                value = ret[1];
            } else {
                value = row[key];
            }

            if (Array.isArray(value))
                value = value[col_idx];

            if (value === true) {
                continue;
            } else if (value === false) {
                return;
            }

            if (Array.isArray(value)) {
                const values = value;
                for (const value of values) {
                    if (ptr[value] === undefined)
                        ptr[value] = {};
                    template[key] = value;

                    aggregateRec(row, row_ptrs, ptr[value], col_idx, key_idx + 1);
                }

                return;
            }

            if (ptr[value] === undefined)
                ptr[value] = {};
            template[key] = value;

            ptr = ptr[value];
        }

        if (!ptr.valid) {
            Object.assign(ptr, template);
            list.push(ptr);
        }

        func(row, col_idx, !row_ptrs.has(ptr), ptr);
        row_ptrs.add(ptr);
    }

    this.aggregate = function(rows) {
        for (const row of rows) {
            const max_idx = row.mono_count.length;

            let row_ptrs = new Set;
            for (let i = 0; i < max_idx; i++)
                aggregateRec(row, row_ptrs, map, i, 0);
        }
    };

    this.getList = function() { return list; };

    if (!Array.isArray(by))
        by = Array.prototype.slice.call(arguments, 2);
    template = Object.assign({valid: true}, template);
}
