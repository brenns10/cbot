FROM alpine:3.14
RUN echo 'http://localhost:8080' >> /etc/apk/repositories
ADD stephen@brennan.io.rsa.pub /etc/apk/keys/
RUN apk add cbot openjdk8-jre gcompat
RUN adduser --disabled-password --uid 1002 cbot && \
    mkdir -p /var/run/signald && \
    chown cbot /var/run/signald
COPY signald /opt/signald
COPY --chmod=755 run.sh /bin/

USER cbot
WORKDIR /home/cbot
CMD ["/bin/run.sh"]
