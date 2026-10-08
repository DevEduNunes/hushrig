idf_component_register(
    SRCS main.cpp audio.cpp net.cpp storage.cpp battery.cpp
    # "../../src" = DSP do app (dsp/*.h), compartilhado sem cópia
    INCLUDE_DIRS "." "../../src"
    REQUIRES driver esp_adc esp_wifi esp_event esp_netif esp_http_server nvs_flash json esp_timer
    EMBED_TXTFILES "web/index.html")
