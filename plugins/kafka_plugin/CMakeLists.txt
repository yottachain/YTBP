if(BUILD_KAFKA_PLUGIN)
  file(GLOB HEADERS "include/eosio/kafka_plugin/*.hpp")
  include_directories("/usr/local/include")
  #LINK_LIBRARIES("/usr/local/lib/librdkafka.so" "/usr/local/lib/librdkafka.so.1")
  LINK_LIBRARIES("/usr/local/lib/librdkafka.a")
  link_directories("/usr/local/lib")
  add_library( kafka_plugin
               kafka_plugin.cpp kafka_producer.cpp
               ${HEADERS} )
  target_include_directories(kafka_plugin
          PUBLIC "include"
          )
  target_link_libraries(kafka_plugin
          PUBLIC chain_plugin eosio_chain appbase fc
          )
else()
  message("kafka_plugin not selected and will be omitted.")
endif()
