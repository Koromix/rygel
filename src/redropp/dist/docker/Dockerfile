FROM --platform=$BUILDPLATFORM rygel/debian12 AS build
ADD repo.tar repo
RUN repo/bootstrap.sh
COPY versions.txt repo/versions.txt
ARG TARGETARCH
RUN sed -i -e 's/Port = [0-9]\+/Port = 80/' repo/src/redropp/server/config.ini
RUN CPPFLAGS="-DDEFAULT_DIRECTORY=/ -DDEFAULT_PORT=80" repo/felix -C repo/FelixBuild.ini -ve -pParanoid --version_file repo/versions.txt --host=$TARGETARCH:clang-18:lld-18 redropp
COPY isolate.py isolate.py
RUN ./isolate.py -p /app -O app repo/bin/Paranoid/redropp
RUN sed -e 's/^\([A-Z\[]\)/# \1/' repo/src/redropp/server/config.ini > config.ini
RUN mkdir empty

FROM scratch AS app
COPY --from=build app app
COPY --from=build empty config
COPY --from=build empty data
COPY --from=build empty tmp
COPY --from=build config.ini config/redropp.ini

FROM scratch
COPY --from=app . .
CMD ["app/redropp", "-C", "/config/redropp.ini", "--sandbox"]
