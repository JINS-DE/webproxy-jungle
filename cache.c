// cache.c
#include "cache.h"

// 캐시 리스트의 헤드와 테일 노드
static cache_node *head = NULL;  // 가장 최근에 접근된 노드
static cache_node *tail = NULL;  // 가장 오래된 노드

// 현재 캐시의 크기와 최대 크기
static size_t current_size = 0;
static size_t max_cache_size = 0;

// 캐시를 초기화하는 함수
void cache_init(size_t max_size) {
    max_cache_size = max_size;
    head = NULL;
    tail = NULL;
    current_size = 0;
}

// 캐시 항목을 검색하는 함수 (아직 구현되지 않음)
const char *cache_get(const char *key) {
    cache_node *node = head;

    while (node != NULL){
        
    }
    
    return NULL;
}

// 캐시 항목을 추가하는 함수 (아직 구현되지 않음)
void cache_put(const char *key, const char *value, size_t size) {
    // 나중에 구현될 부분
}

// 캐시를 해제하는 함수
void cache_free() {
    cache_node *node = head;
    while (node != NULL) {
        cache_node *next = node->next;
        free(node->key);
        free(node->value);
        free(node);
        node = next;
    }
    head = tail = NULL;
    current_size = 0;
}
