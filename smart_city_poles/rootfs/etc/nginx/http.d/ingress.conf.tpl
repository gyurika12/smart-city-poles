# Rendered at startup by the nginx s6 service (token injected from env).

upstream ha_core {
    server supervisor:80;
}

server {
    listen 8099;

    # Only the Supervisor's ingress proxy may talk to this server
    allow 172.30.32.2;
    deny all;

    root /www;
    index index.html;

    # -------------------------------------------------------------------------
    # Home Assistant API through the add-on. The Supervisor token is injected
    # server-side, so only the few endpoints the dashboard needs are exposed -
    # everything else under /api/ is refused.
    # -------------------------------------------------------------------------
    set $ha_token "Bearer __SUPERVISOR_TOKEN__";

    # Read: current states and 24 h history
    location = /api/states {
        limit_except GET { deny all; }
        proxy_pass http://ha_core/core$request_uri;
        proxy_set_header Authorization $ha_token;
    }
    location ^~ /api/history/period/ {
        limit_except GET { deny all; }
        proxy_pass http://ha_core/core$request_uri;
        proxy_set_header Authorization $ha_token;
    }
    # Entity discovery (device_entities of each pole's presence sensor)
    location = /api/template {
        limit_except POST { deny all; }
        proxy_pass http://ha_core/core$request_uri;
        proxy_set_header Authorization $ha_token;
    }
    # Control: LED mode select, LED switch, lamps
    location ~ ^/api/services/(select/select_option|switch/turn_on|switch/turn_off|light/turn_on|light/turn_off)$ {
        limit_except POST { deny all; }
        proxy_pass http://ha_core/core$request_uri;
        proxy_set_header Authorization $ha_token;
    }
    location /api/ {
        return 403;
    }

    # Generated from the add-on options on every start
    location = /config.json {
        add_header Cache-Control "no-store";
    }

    location /vendor/ {
        expires 30d;
    }

    location / {
        # Single-file dashboard: every path (incl. the ingress prefix) serves it.
        add_header Cache-Control "no-cache";
        try_files $uri $uri/ /index.html;
    }
}
