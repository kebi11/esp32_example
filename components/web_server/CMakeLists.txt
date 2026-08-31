idf_component_register(
    SRCS
        "web_server.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        app_config
        gnss
        wifi_manager
        system_monitor
        esp_http_server
        json
    # TODO(Phase 6): 取消下面的注释，把 web/ 下的静态文件嵌入固件
    # EMBED_FILES
    #     "../../web/index.html"
    #     "../../web/style.css"
    #     "../../web/app.js"
)
