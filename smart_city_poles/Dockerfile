ARG BUILD_FROM
FROM ${BUILD_FROM}

# nginx serves the dashboard through the add-on ingress
RUN apk add --no-cache nginx \
    && rm -f /etc/nginx/http.d/default.conf \
    && mkdir -p /run/nginx

# Front-end libraries are bundled into the image (pinned + checksummed), so the
# dashboard works on networks without internet access. Only the map tiles
# still come from the internet.
ARG LEAFLET_VERSION=1.9.4
ARG CHARTJS_VERSION=4.4.0
RUN mkdir -p /www/vendor \
    && curl -fsSL -o /www/vendor/leaflet.js  "https://cdn.jsdelivr.net/npm/leaflet@${LEAFLET_VERSION}/dist/leaflet.js" \
    && curl -fsSL -o /www/vendor/leaflet.css "https://cdn.jsdelivr.net/npm/leaflet@${LEAFLET_VERSION}/dist/leaflet.css" \
    && curl -fsSL -o /www/vendor/chart.umd.min.js "https://cdn.jsdelivr.net/npm/chart.js@${CHARTJS_VERSION}/dist/chart.umd.min.js" \
    && cd /www/vendor \
    && printf '%s\n' \
        "db49d009c841f5ca34a888c96511ae936fd9f5533e90d8b2c4d57596f4e5641a  leaflet.js" \
        "a7837102824184820dfa198d1ebcd109ff6d0ff9a2672a074b9a1b4d147d04c6  leaflet.css" \
        "0e2326c6868072bec1592760c6729043caeea2960a2b46cee6a2192aac6abff0  chart.umd.min.js" \
        | sha256sum -c -

# Copy root filesystem (s6 services, nginx conf, dashboard, package templates)
COPY rootfs /
RUN chmod a+x /usr/bin/smart-poles-render
