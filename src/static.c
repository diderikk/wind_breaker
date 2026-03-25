#include "static.h"
#include "stdlib.h"

int ensure_capacity_(const char* callee, buffer* buffer, unsigned long capacity) {
  unsigned long old_capacity = buffer->capacity;
  while(buffer->capacity < capacity) {
    buffer->capacity = (buffer->capacity < DEFAULT_BUFFER_SIZE) ? DEFAULT_BUFFER_SIZE : buffer->capacity * 2;
  }
  if(buffer->capacity > old_capacity) {
    buffer->data = realloc(buffer->data, sizeof(char) * buffer->capacity);
    if(buffer->data == NULL)
      return -1;
  }

  return buffer->capacity;
}

char *uri_to_file_name(const uri_token_t uri) {
  if (strcmp(uri[0], "") == 0) {
    return "index.html";
  } else if (strncmp(uri[0], "favicon", strlen("favicon")) == 0) {
    return "favicon.png";
  } else if (strcmp(uri[0], "projects") == 0 && strlen(uri[1]) > 0 &&
             uri[2][0] == 0) {
    return "project.html";
  } else if (strcmp(uri[0], "posts") == 0 && strlen(uri[1]) > 0 &&
             uri[2][0] == 0) {
    return "post.html";
  } else if (strcmp(uri[0], "style.css") == 0) {
    return "style.css";
  } else if (strcmp(uri[0], "0975d534f3bf2b637270fa1ddf405661bbac3759a8dace84d0"
                            "7b685050c6e628.png") == 0) {
    return "0975d534f3bf2b637270fa1ddf405661bbac3759a8dace84d07b685050c6e628."
           "png";
  } else if (strcmp(uri[0], "69b03fbd0ca6cabdffd278e4baf016b29850b744701bff9dc9"
                            "3454eb84a09e6a.png") == 0) {
    return "69b03fbd0ca6cabdffd278e4baf016b29850b744701bff9dc93454eb84a09e6a."
           "png";
  } else if (strcmp(uri[0], "145d91af1f648f5a56b13d18c19de330ad52b7657f6962c71d"
                            "e81042a0f5b4dc.png") == 0) {
    return "145d91af1f648f5a56b13d18c19de330ad52b7657f6962c71de81042a0f5b4dc."
           "png";
  } else if (strcmp(uri[0], "70fa446d6596731fdfa30f73647f4118eedfa3d78492a25f40"
                            "0986d6cc072e5c.png") == 0) {
    return "70fa446d6596731fdfa30f73647f4118eedfa3d78492a25f400986d6cc072e5c."
           "png";
  } else if (strcmp(uri[0], "1d25c8476099865726b9a1e2123bd3d0f282e740247cd9c7f9"
                            "034a6ab92a12bd.png") == 0) {
    return "1d25c8476099865726b9a1e2123bd3d0f282e740247cd9c7f9034a6ab92a12bd."
           "png";
  } else if (strcmp(uri[0], "8153f517515384e8234f5c9f3f9e768e217c7e693a82035845"
                            "fbee5e1747f666.png") == 0) {
    return "8153f517515384e8234f5c9f3f9e768e217c7e693a82035845fbee5e1747f666."
           "png";
  } else if (strcmp(uri[0], "210a8c166fdf0ccd14563af74a3907425888ef4d80a2a8526e"
                            "de2a95ea687811.png") == 0) {
    return "210a8c166fdf0ccd14563af74a3907425888ef4d80a2a8526ede2a95ea687811."
           "png";
  } else if (strcmp(uri[0], "8648ad9cbe5bd20a40d762c8e3d7f1b91c8d431dd916e17e4e"
                            "ebbdee108c7d52.png") == 0) {
    return "8648ad9cbe5bd20a40d762c8e3d7f1b91c8d431dd916e17e4eebbdee108c7d52."
           "png";
  } else if (strcmp(uri[0], "8c740dc36c91eceffb6c253223a539daf9183f221e2797ebae"
                            "cdd1460ad34004.gif") == 0) {
    return "8c740dc36c91eceffb6c253223a539daf9183f221e2797ebaecdd1460ad34004."
           "gif";
  } else if (strcmp(uri[0], "3a7a9d67fc25b9320a3d9951ea173a704f4d709504ddb63e7b"
                            "68ce32224af2a8.png") == 0) {
    return "3a7a9d67fc25b9320a3d9951ea173a704f4d709504ddb63e7b68ce32224af2a8."
           "png";
  } else if (strcmp(uri[0], "8eb84b4dc5a3348241ab32975518c0bf172fe5672d49eba93c"
                            "4035c4122ca123.png") == 0) {
    return "8eb84b4dc5a3348241ab32975518c0bf172fe5672d49eba93c4035c4122ca123."
           "png";
  } else if (strcmp(uri[0], "3ba0dd359074d43ae207bcbc1123dc6877bf960808ee48f1fa"
                            "857928608a65b1.png") == 0) {
    return "3ba0dd359074d43ae207bcbc1123dc6877bf960808ee48f1fa857928608a65b1."
           "png";
  } else if (strcmp(uri[0], "999d31eaaad2c92075ba67a078e7c5e46e0f994eb9e34b403f"
                            "0d1fa05655d293.png") == 0) {
    return "999d31eaaad2c92075ba67a078e7c5e46e0f994eb9e34b403f0d1fa05655d293."
           "png";
  } else if (strcmp(uri[0], "3bbe7d9a652ba2780faecceb9e4fea270541de2a53209361fa"
                            "450a82b6c55d21.png") == 0) {
    return "3bbe7d9a652ba2780faecceb9e4fea270541de2a53209361fa450a82b6c55d21."
           "png";
  } else if (strcmp(uri[0], "a29473724c503d01d2362544a422dc9400917c2885388acbc5"
                            "ce6104344b1c2d.png") == 0) {
    return "a29473724c503d01d2362544a422dc9400917c2885388acbc5ce6104344b1c2d."
           "png";
  } else if (strcmp(uri[0], "3ecffa9f8be1d43cd9a7b309514ef456cc6b56e8654503f77d"
                            "dfe26a7237590a.png") == 0) {
    return "3ecffa9f8be1d43cd9a7b309514ef456cc6b56e8654503f77ddfe26a7237590a."
           "png";
  } else if (strcmp(uri[0], "a51f173624c7bb1f1605e6b6be6c28620a9411b2e8e7bd6a4d"
                            "d2f3944bccb489.png") == 0) {
    return "a51f173624c7bb1f1605e6b6be6c28620a9411b2e8e7bd6a4dd2f3944bccb489."
           "png";
  } else if (strcmp(uri[0], "3efed1a4e31cfc31aa8309a8335b9e1eced1ad8f4df6767ea0"
                            "8a8194da44d9c9.png") == 0) {
    return "3efed1a4e31cfc31aa8309a8335b9e1eced1ad8f4df6767ea08a8194da44d9c9."
           "png";
  } else if (strcmp(uri[0], "adbc5ee112e14f8e5de7d00433a26929536b7173067e7e7dfa"
                            "515c6ff2f56f09.png") == 0) {
    return "adbc5ee112e14f8e5de7d00433a26929536b7173067e7e7dfa515c6ff2f56f09."
           "png";
  } else if (strcmp(uri[0], "3ffc46fac626f6cd38b0f714aad34bcfc5d1839103dcb4c0c8"
                            "ddb80459bc124a.png") == 0) {
    return "3ffc46fac626f6cd38b0f714aad34bcfc5d1839103dcb4c0c8ddb80459bc124a."
           "png";
  } else if (strcmp(uri[0], "cb4157a69bfdcef2448645ddab47e60391ba03e5a2caaa7480"
                            "711a47362b6478.png") == 0) {
    return "cb4157a69bfdcef2448645ddab47e60391ba03e5a2caaa7480711a47362b6478."
           "png";
  } else if (strcmp(uri[0], "41332ce122eb61d15209743301eeb414ac602bfabde7708139"
                            "36bd1826665eca.png") == 0) {
    return "41332ce122eb61d15209743301eeb414ac602bfabde770813936bd1826665eca."
           "png";
  } else if (strcmp(uri[0], "dd10071f8d4e0d4feb4a16e89416ccf077455508e169be446c"
                            "6f03d6cf0c7830.png") == 0) {
    return "dd10071f8d4e0d4feb4a16e89416ccf077455508e169be446c6f03d6cf0c7830."
           "png";
  } else if (strcmp(uri[0], "427846088631e78a1dcdab8eef163ca3ea69a700246752cf38"
                            "357553cca40f17.png") == 0) {
    return "427846088631e78a1dcdab8eef163ca3ea69a700246752cf38357553cca40f17."
           "png";
  } else if (strcmp(uri[0], "df8e92aa091dd2f54114fd8c417d0fbffd3d8d3351a5c2f09f"
                            "f0b14207175756.png") == 0) {
    return "df8e92aa091dd2f54114fd8c417d0fbffd3d8d3351a5c2f09ff0b14207175756."
           "png";
  } else if (strcmp(uri[0], "4c52669371549f48d7c9e9a2ce0ff5c74e2d14d3da77b880df"
                            "00b9a1dd80e3e5.png") == 0) {
    return "4c52669371549f48d7c9e9a2ce0ff5c74e2d14d3da77b880df00b9a1dd80e3e5."
           "png";
  } else if (strcmp(uri[0], "e4880dbd9ffd0651623e8f16b266e24baad255aec6fe6cfdcd"
                            "abf3155b08e5c2.png") == 0) {
    return "e4880dbd9ffd0651623e8f16b266e24baad255aec6fe6cfdcdabf3155b08e5c2."
           "png";
  } else if (strcmp(uri[0], "4fc0cac0e8a608b8622e6bf4265dfd29bf6c1584330a461a0d"
                            "914f5b2a9c358b.png") == 0) {
    return "4fc0cac0e8a608b8622e6bf4265dfd29bf6c1584330a461a0d914f5b2a9c358b."
           "png";
  } else if (strcmp(uri[0], "e6a6d5ba2c72380dc83a4db0ce941a399e2fbc0e644a6f91b5"
                            "e3686e163ff1c0.png") == 0) {
    return "e6a6d5ba2c72380dc83a4db0ce941a399e2fbc0e644a6f91b5e3686e163ff1c0."
           "png";
  } else if (strcmp(uri[0], "57d7abd64acf4b4570ddc963d25a51c66268b6c77858c6bcd8"
                            "5eb255d5daa363.png") == 0) {
    return "57d7abd64acf4b4570ddc963d25a51c66268b6c77858c6bcd85eb255d5daa363."
           "png";
  } else if (strcmp(uri[0], "e6f1244905e3e7b87c73d31adec2531a6cf32788815cfec10b"
                            "3a634c8af32c16.png") == 0) {
    return "e6f1244905e3e7b87c73d31adec2531a6cf32788815cfec10b3a634c8af32c16."
           "png";
  } else if (strcmp(uri[0], "58215b9adc045b1ea48e48bc402af26901a96a8bea4dc4ca54"
                            "cba3b3930d912e.png") == 0) {
    return "58215b9adc045b1ea48e48bc402af26901a96a8bea4dc4ca54cba3b3930d912e."
           "png";
  } else if (strcmp(uri[0], "5bfa0a1a1cc92156513e4f773f8a49e4b8a9ac91bf934c2308"
                            "27861835891438.png") == 0) {
    return "5bfa0a1a1cc92156513e4f773f8a49e4b8a9ac91bf934c230827861835891438."
           "png";
  } else if (strcmp(uri[0], "ff7da19901c921781f87d5ad417f4bbd9246602c732e6dfd2f"
                            "9313c0100b1ba5.png") == 0) {
    return "ff7da19901c921781f87d5ad417f4bbd9246602c732e6dfd2f9313c0100b1ba5."
           "png";
  } else {
    return NULL;
  }
}
