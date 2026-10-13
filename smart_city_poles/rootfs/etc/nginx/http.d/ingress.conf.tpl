# Rendered at startup by the nginx s6 service (token injected from env).
map $http_upgrade $scp_connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    listen 8099;

    root /www;
    index index.html;

    # Home Assistant API through the add-on. The Supervisor token is injected
    # server-side, so the dashboard needs no user token when opened via ingress.
    location /api/ {
        proxy_pass http://supervisor/core;
        proxy_set_header Authorization "Bearer __SUPERVISOR_TOKEN__";
        proxy_set_header Host $host;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $scp_connection_upgrade;
        proxy_read_timeout 3600s;
        proxy_buffering off;
    }

    location / {
        # Single-file dashboard: every path (incl. the ingress prefix) serves it.
        try_files $uri $uri/ /index.html;
    }

    location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff2?)$ {
        expires 1h;
    }
}
