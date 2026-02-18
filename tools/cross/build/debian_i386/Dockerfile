FROM debian:13
COPY stage1.sh /
RUN /stage1.sh
COPY resolv.conf interfaces stage2.sh stage3.sh /
