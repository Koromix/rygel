import { _adapters } from 'chart.js/auto';

const FORMATS = {
    datetime: 'lll',
    millisecond: 'H:mm:ss.SSS',
    second: 'H:mm:ss',
    minute: 'H:mm',
    hour: 'H',
    day: 'L',
    week: 'll',
    month: 'MMM YYYY',
    quarter: '[Q]Q YYYY',
    year: 'YYYY'
};

function adaptDayjs(dayjs) {
    _adapters._date.override({
        _id: 'dayjs',

        formats: function() {
            return FORMATS;
        },

        parse: function(value, format) {
            let date = null;

            if (typeof value == 'string') {
                date = dayjs(value, format);
            } else if (value != null) {
                date = dayjs(value);
            }
            if (date != null && !date.isValid())
                date = null;

            return date;
        },

        format: function(time, format) {
            time = dayjs(time);
            return time.format(format);
        },

        add: function(time, amount, unit) {
            time = dayjs(time);
            return time.add(amount, unit).valueOf();
        },

        diff: function(time, since, unit) {
            time = dayjs(time);
            since = dayjs(since);

            return time.diff(since, unit).valueOf();
        },

        startOf: function(time, unit, weekday) {
            time = dayjs(time);

            if (unit === 'isoWeek') {
                if (typeof weekday != 'number' || weekday < 0 || weekday > 6)
                    weekday = 1;

                return time.isoWeekday(weekday).startOf('day').valueOf();
            } else {
                return time.startOf(unit).valueOf();
            }
        },

        endOf: function(time, unit) {
            time = dayjs(time);
            return time.endOf(unit).valueOf();
        }
    });
}

export * from 'chart.js/auto';
export { adaptDayjs };
