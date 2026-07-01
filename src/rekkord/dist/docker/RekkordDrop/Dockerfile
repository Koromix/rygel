FROM --platform=$BUILDPLATFORM rygel/debian12 AS build
ADD repo.tar repo
RUN repo/bootstrap.sh
COPY versions.txt repo/versions.txt
ARG TARGETARCH
RUN repo/felix -C repo/FelixBuild.ini -pParanoid --version_file repo/versions.txt --host=$TARGETARCH:clang-18:lld-18 RekkordDrop
COPY isolate.py isolate.py
RUN ./isolate.py -p /app -O app repo/bin/Paranoid/RekkordDrop
RUN mkdir empty

FROM scratch AS app
COPY --from=build app app
COPY --from=build empty config
COPY --from=build empty data
COPY --from=build empty tmp
COPY --from=build repo/src/rekkord/dist/docker/RekkordDrop/RekkordDrop.ini config/RekkordDrop.ini

FROM scratch
COPY --from=app . .
CMD ["app/RekkordDrop", "-C", "/config/RekkordDrop.ini", "--sandbox"]
