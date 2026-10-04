"""Black-box request sequences; expected responses are captured from the baseline."""
import json


def request(method, target, body=None, token=None, content_type=True, headers=None):
    result = {"method": method, "target": target, "headers": dict(headers or {})}
    if body is not None:
        result["body"] = body if isinstance(body, str) else json.dumps(body)
        if content_type:
            result["headers"]["Content-Type"] = "application/json"
    if token is not None:
        result["headers"]["Authorization"] = "Bearer " + token
    return result


def scenarios():
    api = "/api/v1/"
    game = api + "game/"
    cases = []
    for target in [api + "maps", api + "maps/square", api + "maps/missing", game + "join",
                   game + "players", game + "state", game + "player/action", game + "tick",
                   game + "records", api + "unknown"]:
        for method in ["GET", "HEAD", "POST", "PUT", "DELETE"]:
            cases.append(request(method, target))
    for body in ["broken", {}, {"mapId": 42, "userName": "A"}, {"mapId": "square", "userName": ""},
                 {"mapId": "missing", "userName": "A"}]:
        cases.append(request("POST", game + "join", body))
    for name, map_id in [("Alice", "square"), ("Боб", "square"), ("Other", "other")]:
        cases.append(request("POST", game + "join", {"mapId": map_id, "userName": name}))
    for target in ["players", "state"]:
        for token in ["@0", "@2", "a" * 32, "invalid", "g" * 32]:
            cases.append(request("GET", game + target, token=token))
        cases.append(request("HEAD", game + target, token="@0"))
        cases.append(request("GET", game + target, headers={"Authorization": "Basic " + "a" * 32}))
    for direction in ["R", "X", "", "U", "D", "L"]:
        cases.append(request("POST", game + "player/action", {"move": direction}, "@0"))
    for body in ["broken", {}, {"move": 42}]:
        cases.append(request("POST", game + "player/action", body, "@0"))
    cases.append(request("POST", game + "player/action", {"move": "R"}, "a" * 32))
    cases.append(request("POST", game + "player/action", {"move": "R"}, "@0", False))
    for body in ["broken", {}, {"timeDelta": "10"}, {"timeDelta": 1.5}]:
        cases.append(request("POST", game + "tick", body))
    cases += [request("POST", game + "player/action", {"move": "R"}, "@0"),
              request("POST", game + "tick", {"timeDelta": 1000}),
              request("GET", game + "state", token="@0"),
              request("POST", game + "tick", {"timeDelta": 2500}),
              request("GET", game + "state", token="@0"),
              request("GET", game + "players", token="@0"),
              request("GET", game + "state", token="@1")]
    for query in ["", "?start=0&maxItems=1", "?start=1&maxItems=100", "?maxItems=101",
                  "?maxItems=0", "?start=nonsense&maxItems=abc", "?maxItems=2suffix", "?ignored=1"]:
        cases.append(request("GET", game + "records" + query))
    for target in ["/api//v1/maps/", "/api/v1/maps/%73quare", "/api/v2/maps", "/", "/missing-file.txt"]:
        cases.append(request("GET", target))
    auto = [request("POST", game + "join", {"mapId": "square", "userName": "Auto"}),
            request("POST", game + "tick", {"timeDelta": 100}),
            request("POST", game + "player/action", {"move": "R"}, "@0"),
            {"autoTick": 500}, request("GET", game + "state", token="@0")]
    snapshots = [request("POST", game + "join", {"mapId": "square", "userName": "Saved"}),
                 request("POST", game + "player/action", {"move": "R"}, "@0"),
                 request("POST", game + "tick", {"timeDelta": 1000}),
                 request("GET", game + "state", token="@0"), {"snapshot": True},
                 request("GET", game + "state", token="@0"),
                 request("POST", game + "player/action", {"move": "R"}, "@0"),
                 request("POST", game + "tick", {"timeDelta": 500}),
                 request("GET", game + "state", token="@0")]
    return {"manual": cases, "auto": auto, "snapshots": snapshots}
