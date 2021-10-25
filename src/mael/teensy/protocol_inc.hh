// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#ifndef MESSAGE
    #error Please define MESSAGE() before including protocol_inc.hh
#endif

MESSAGE(Imu, {
    Vec3<double> position;
    Vec3<double> orientation;
    Vec3<double> acceleration;
})

MESSAGE(Drive, {
    Vec2<double> speed;
    double w;
})

#undef MESSAGE
