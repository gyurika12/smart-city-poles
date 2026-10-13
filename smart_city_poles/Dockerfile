ARG BUILD_FROM
FROM ${BUILD_FROM}

# nginx serves the dashboard through the add-on ingress
RUN apk add --no-cache nginx \
    && rm -f /etc/nginx/http.d/default.conf \
    && mkdir -p /run/nginx

# Copy root filesystem (s6 services, nginx conf, dashboard, package templates)
COPY rootfs /
