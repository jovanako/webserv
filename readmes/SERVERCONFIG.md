# `_clientMaxBodySize(1048576)`

The default `clientMaxBodySize` is set to `1048576` bytes in the `ServerConfig` constructor because that value equals exactly 1 Megabyte (1024 * 1024 bytes).

The project guidelines explicitly recommend taking inspiration from the `server` section of the NGINX configuration file. In standard NGINX configurations, the default limit for `client_max_body_size` is 1 MB. By hardcoding `1048576`, your server accurately mirrors NGINX's default behavior and provides a sensible, secure fallback size when a `client_max_body_size` directive is not explicitly defined in the configuration file.