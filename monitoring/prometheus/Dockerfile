FROM prom/prometheus:v3.15.0
COPY prometheus.yml /etc/prometheus/prometheus.yml
RUN promtool check config /etc/prometheus/prometheus.yml
