FROM rygel/debian12 AS build
ADD repo.tar repo
RUN repo/bootstrap.sh
COPY versions.txt repo/versions.txt
RUN repo/felix -C repo/FelixBuild.ini -pParanoid --version_file versions.txt --host=:clang-18:lld-18 goupile
COPY isolate.sh isolate.sh
RUN ./isolate.sh repo/bin/Paranoid/goupile app
RUN mkdir empty

FROM scratch AS app
COPY --from=build app app
COPY --from=build repo/src/goupile/dist/docker/goupile.ini app/goupile.ini
COPY --from=build empty data
COPY --from=build empty tmp

FROM scratch
COPY --from=app . .
CMD ["app/goupile", "-C", "/app/goupile.ini", "--sandbox"]
