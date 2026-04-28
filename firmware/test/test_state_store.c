#include "unity.h"
#include "state_store.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_init_state_is_empty(void) {
    state_t s;
    state_init(&s);
    TEST_ASSERT_EQUAL(0, s.sessions_count);
    TEST_ASSERT_FALSE(s.alert.active);
    TEST_ASSERT_FALSE(s.quota.present);
}

void test_alert_apply_and_clear(void) {
    state_t s; state_init(&s);
    state_apply_alert(&s,
      "{\"v\":1,\"type\":\"alert\",\"id\":\"a1\",\"kind\":\"approval\","
      "\"sessionId\":\"s1\",\"sessionName\":\"x\",\"text\":\"rm -rf\"}");
    TEST_ASSERT_TRUE(s.alert.active);
    TEST_ASSERT_EQUAL_STRING("a1", s.alert.id);
    TEST_ASSERT_EQUAL_STRING("rm -rf", s.alert.text);
    state_apply_alert_clear(&s, "a1");
    TEST_ASSERT_FALSE(s.alert.active);
}

void test_state_msg_replaces_sessions(void) {
    state_t s; state_init(&s);
    state_apply_state_msg(&s,
      "{\"v\":1,\"type\":\"state\","
      "\"sessions\":{\"active\":2,\"items\":["
        "{\"id\":\"a\",\"name\":\"x\",\"path\":\"/p\",\"status\":\"active\","
         "\"lastTool\":\"Edit\",\"lastTarget\":\"f.ts\","
         "\"lastTime\":100,\"toolCount\":2,\"startTime\":50},"
        "{\"id\":\"b\",\"name\":\"y\",\"path\":\"/q\",\"status\":\"idle\","
         "\"lastTool\":\"Read\",\"lastTarget\":\"\","
         "\"lastTime\":80,\"toolCount\":1,\"startTime\":40}"
      "]},\"quota\":null,\"alert\":null}");
    TEST_ASSERT_EQUAL(2, s.sessions_count);
    TEST_ASSERT_EQUAL(2, s.sessions_total);
    TEST_ASSERT_EQUAL_STRING("a", s.sessions[0].id);
    TEST_ASSERT_EQUAL(SS_ACTIVE, s.sessions[0].status);
    TEST_ASSERT_EQUAL(SS_IDLE,   s.sessions[1].status);
}

void test_state_msg_truncates_at_5(void) {
    state_t s; state_init(&s);
    char buf[2048] = "{\"v\":1,\"type\":\"state\",\"sessions\":{\"active\":7,\"items\":[";
    for (int i = 0; i < 7; i++) {
        char one[256];
        snprintf(one, sizeof(one),
            "%s{\"id\":\"i%d\",\"name\":\"n%d\",\"path\":\"/\",\"status\":\"active\","
            "\"lastTool\":\"E\",\"lastTarget\":\"\",\"lastTime\":%d,\"toolCount\":1,\"startTime\":1}",
            i ? "," : "", i, i, 100 + i);
        strcat(buf, one);
    }
    strcat(buf, "]},\"quota\":null,\"alert\":null}");
    state_apply_state_msg(&s, buf);
    TEST_ASSERT_EQUAL(5, s.sessions_count);
    TEST_ASSERT_EQUAL(7, s.sessions_total);
}

void test_session_update_patches_existing(void) {
    state_t s; state_init(&s);
    state_apply_state_msg(&s,
      "{\"v\":1,\"type\":\"state\",\"sessions\":{\"active\":1,\"items\":["
        "{\"id\":\"a\",\"name\":\"x\",\"path\":\"/p\",\"status\":\"active\","
         "\"lastTool\":\"Edit\",\"lastTarget\":\"f.ts\","
         "\"lastTime\":100,\"toolCount\":2,\"startTime\":50}]},"
      "\"quota\":null,\"alert\":null}");
    state_apply_session_update(&s,
      "{\"v\":1,\"type\":\"session-update\",\"id\":\"a\","
      "\"patch\":{\"lastTool\":\"Bash\",\"lastTarget\":\"git\",\"lastTime\":110,\"toolCount\":3}}");
    TEST_ASSERT_EQUAL_STRING("Bash", s.sessions[0].last_tool);
    TEST_ASSERT_EQUAL_STRING("git",  s.sessions[0].last_target);
    TEST_ASSERT_EQUAL(110, s.sessions[0].last_time);
    TEST_ASSERT_EQUAL(3,   s.sessions[0].tool_count);
}

void test_session_update_inserts_unknown_id(void) {
    state_t s; state_init(&s);
    state_apply_session_update(&s,
      "{\"v\":1,\"type\":\"session-update\",\"id\":\"new\","
      "\"patch\":{\"name\":\"NN\",\"status\":\"active\",\"lastTool\":\"Read\","
      "\"lastTarget\":\"x\",\"lastTime\":1,\"toolCount\":0}}");
    TEST_ASSERT_EQUAL(1, s.sessions_count);
    TEST_ASSERT_EQUAL_STRING("new", s.sessions[0].id);
    TEST_ASSERT_EQUAL_STRING("NN",  s.sessions[0].name);
}

void test_session_end_removes_existing(void) {
    state_t s; state_init(&s);
    state_apply_session_update(&s,
      "{\"v\":1,\"type\":\"session-update\",\"id\":\"a\","
      "\"patch\":{\"name\":\"x\",\"status\":\"active\",\"lastTool\":\"E\","
      "\"lastTarget\":\"\",\"lastTime\":1,\"toolCount\":0}}");
    state_apply_session_end(&s, "a");
    TEST_ASSERT_EQUAL(0, s.sessions_count);
}

void test_quota_msg_only_updates_quota(void) {
    state_t s; state_init(&s);
    state_apply_session_update(&s,
      "{\"v\":1,\"type\":\"session-update\",\"id\":\"a\","
      "\"patch\":{\"name\":\"x\",\"status\":\"active\",\"lastTool\":\"E\","
      "\"lastTarget\":\"\",\"lastTime\":1,\"toolCount\":0}}");
    state_apply_quota_msg(&s,
      "{\"v\":1,\"type\":\"quota\","
      "\"fiveHour\":{\"utilization\":0.5,\"reset\":100},"
      "\"sevenDay\":{\"utilization\":0.2,\"reset\":200},"
      "\"opus\":{\"utilization\":0.1}}");
    TEST_ASSERT_TRUE(s.quota.present);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, s.quota.five_hour);
    TEST_ASSERT_EQUAL(1, s.sessions_count);
}

int app_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_state_is_empty);
    RUN_TEST(test_alert_apply_and_clear);
    RUN_TEST(test_state_msg_replaces_sessions);
    RUN_TEST(test_state_msg_truncates_at_5);
    RUN_TEST(test_session_update_patches_existing);
    RUN_TEST(test_session_update_inserts_unknown_id);
    RUN_TEST(test_session_end_removes_existing);
    RUN_TEST(test_quota_msg_only_updates_quota);
    return UNITY_END();
}
