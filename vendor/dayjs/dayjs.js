import dayjs from 'dayjs' // ES 2015

import 'dayjs/locale/en';
import 'dayjs/locale/fr';

import utc from 'dayjs/plugin/utc';
import timezone from 'dayjs/plugin/timezone';
import customParseFormat from 'dayjs/plugin/customParseFormat';
import quarterOfYear from 'dayjs/plugin/quarterOfYear';
import advancedFormat from 'dayjs/plugin/advancedFormat';
import localizedFormat from 'dayjs/plugin/localizedFormat';
import isoWeek from 'dayjs/plugin/isoWeek';

dayjs.locale('en');

dayjs.extend(utc);
dayjs.extend(timezone);
dayjs.extend(customParseFormat);
dayjs.extend(quarterOfYear);
dayjs.extend(advancedFormat);
dayjs.extend(localizedFormat);
dayjs.extend(isoWeek);

export default dayjs;
