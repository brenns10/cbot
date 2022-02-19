FROM alpine:3.14
COPY cbot.apk /
RUN apk add --allow-untrusted ./cbot.apk openjdk11-jre gcompat zip libgcc && rm ./cbot.apk
RUN adduser --disabled-password --uid 1002 cbot && \
    mkdir -p /var/run/signald && \
    chown cbot /var/run/signald
COPY signald /opt/signald
RUN wget https://github.com/brenns10/libsignal-client-alpine/raw/master/0.12.3/libsignal_jni.so && \
    zip -d /opt/signald/lib/signal-client*.jar libsignal_jni.so && \
    zip /opt/signald/lib/signal-client*.jar libsignal_jni.so && \
    rm libsignal_jni.so
COPY run.sh /bin/
RUN chmod 755 /bin/run.sh
RUN mkdir /var/cores && chmod 777 /var/cores

USER cbot
WORKDIR /home/cbot
CMD ["/bin/run.sh"]
