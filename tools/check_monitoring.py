"""Exercise the optional monitoring stack through its published HTTP interfaces."""

import base64
import json
import os
import time
import urllib.error
import urllib.parse
import urllib.request


GAME = os.environ.get("GAME_BASE_URL", "http://127.0.0.1:" + os.environ.get("GAME_HTTP_PORT", "8080"))
PROMETHEUS = os.environ.get("PROMETHEUS_URL", "http://127.0.0.1:" + os.environ.get("PROMETHEUS_PORT", "9090"))
GRAFANA = os.environ.get("GRAFANA_URL", "http://127.0.0.1:" + os.environ.get("GRAFANA_PORT", "3000"))


def get(url, *, authorization=None, expected=200):
    headers = {} if authorization is None else {"Authorization": authorization}
    try:
        response = urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=5)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        body = response.read()
        assert response.status == expected, (url, response.status, expected)
        return json.loads(body)


def query(expression):
    result = get(PROMETHEUS + "/api/v1/query?" + urllib.parse.urlencode({"query": expression}))
    assert result["status"] == "success", result
    return result["data"]["result"]


def value(expression):
    series = query(expression)
    return float(series[0]["value"][1]) if series else 0.0


def eventually(condition, description, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if condition():
                print("PASS: " + description)
                return
        except (OSError, ValueError, AssertionError):
            pass
        time.sleep(1)
    raise AssertionError("Timed out: " + description)


def main():
    eventually(lambda: get(GAME + "/api/v1/maps"), "game is ready")
    eventually(lambda: value('up{job="dog-story"}') == 1, "Prometheus scrapes the log exporter")
    eventually(lambda: value('up{job="node"}') == 1, "Prometheus scrapes the resource exporter")
    eventually(lambda: value("dog_story_log_readable") == 1, "exporter can read the game log")
    eventually(lambda: value("count(node_cpu_seconds_total)") > 0, "host CPU metrics reach Prometheus")
    eventually(lambda: value("node_memory_MemTotal_bytes") > 0, "host memory metrics reach Prometheus")
    eventually(lambda: value("count(node_filesystem_size_bytes)") > 0, "host disk metrics reach Prometheus")

    successful = value('sum(dog_story_http_requests_total{status_class="2xx"})')
    failed = value('sum(dog_story_http_requests_total{status_class="4xx"})')
    observations = value("sum(dog_story_http_request_duration_seconds_count)")
    for _ in range(20):
        get(GAME + "/api/v1/maps")
    for _ in range(5):
        get(GAME + "/api/v1/monitoring-check-missing", expected=400)
    eventually(lambda: value('sum(dog_story_http_requests_total{status_class="2xx"})') >= successful + 20,
               "successful requests are counted")
    eventually(lambda: value('sum(dog_story_http_requests_total{status_class="4xx"})') >= failed + 5,
               "client errors are counted")
    eventually(lambda: value("sum(dog_story_http_request_duration_seconds_count)") >= observations + 25,
               "latency histogram observes the requests")
    assert value("sum(dog_story_http_request_duration_seconds_sum)") >= 0
    assert value("dog_story_log_parse_errors_total") == 0

    credentials = (os.environ.get("GRAFANA_ADMIN_USER", "admin") + ":" +
                   os.environ["GRAFANA_ADMIN_PASSWORD"]).encode()
    authorization = "Basic " + base64.b64encode(credentials).decode()
    eventually(lambda: get(GRAFANA + "/api/health")["database"] == "ok", "Grafana is ready")
    dashboards = get(GRAFANA + "/api/search?type=dash-db", authorization=authorization)
    assert dashboards, "No provisioned dashboard"
    dashboard = get(GRAFANA + "/api/dashboards/uid/" + dashboards[0]["uid"], authorization=authorization)
    assert len(dashboard["dashboard"]["panels"]) >= 4, "Dashboard is incomplete"
    for panel in dashboard["dashboard"]["panels"]:
        for target in panel.get("targets", []):
            query(target["expr"].replace("$__rate_interval", "1m"))
    sources = get(GRAFANA + "/api/datasources", authorization=authorization)
    prometheus_sources = [source for source in sources if source["type"] == "prometheus"]
    assert prometheus_sources, "Prometheus datasource is missing"
    for source in prometheus_sources:
        health = get(GRAFANA + "/api/datasources/uid/" + source["uid"] + "/health", authorization=authorization)
        assert health["status"] == "OK", health
    print("PASS: provisioned Grafana dashboard and Prometheus datasource")


if __name__ == "__main__":
    main()
