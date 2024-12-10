#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h> // HTTP 요청을 위해 libcurl 사용
#include <string.h>

#define TRIGGER 24
#define ECHO 23

// 초 단위 시간을 반환하는 함수
double get_time_in_seconds() {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return spec.tv_sec + (spec.tv_nsec / 1.0e9);
}

// 상태 변경 시 HTTP 요청 전송
void send_http_request(int percentage) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        // 요청 URL 설정
        char url[256];
        snprintf(url, sizeof(url), "http://59.187.251.226:54549/toilet/towel?num=%d", percentage);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

        // HTTP 요청 전송
        res = curl_easy_perform(curl);

        // 요청 결과 확인
        if (res != CURLE_OK) {
            fprintf(stderr, "HTTP 요청 실패: %s\n", curl_easy_strerror(res));
        } else {
            printf("HTTP 요청 전송 완료: %d%%\n", percentage);
        }

        // curl 정리
        curl_easy_cleanup(curl);
    }
}

int main() {
    double totalLength=30; // 전체 길이를 저장할 변수
    int lastPercentage = -1; // 이전 퍼센티지 값 (-1로 초기화)

    // 사용자로부터 전체 길이를 입력받음
    // printf("전체 길이를 입력하세요 (cm): ");
    // scanf("%lf", &totalLength);

    if (totalLength <= 0) {
        printf("전체 길이는 0보다 커야 합니다.\n");
        return 1;
    }

    // WiringPi 초기화
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi 초기화 실패\n");
        return 1;
    }

    // GPIO 핀 설정
    pinMode(TRIGGER, OUTPUT);
    pinMode(ECHO, INPUT);
    digitalWrite(TRIGGER, LOW);
    usleep(100000); // 100ms 대기

    printf("초음파 센서 시작...\n");

    while (1) {
        // TRIGGER 핀으로 초음파 신호 전송
        digitalWrite(TRIGGER, HIGH);
        usleep(2000000); // 10µs 유지
        digitalWrite(TRIGGER, LOW);

        // ECHO 핀이 HIGH로 변하기를 대기
        while (digitalRead(ECHO) == LOW);
        double startTime = get_time_in_seconds();

        // ECHO 핀이 LOW로 변하기를 대기
        while (digitalRead(ECHO) == HIGH);
        double endTime = get_time_in_seconds();

        // 초음파 신호의 왕복 시간 계산
        double period = endTime - startTime;

        // 거리 계산 (cm 단위)
        double measuredDistance = (period * 1000000) / 58.0;

        // 남은 퍼센티지 계산
        int percentage = (int)(((totalLength - measuredDistance) / totalLength) * 100);

        if (percentage < 0) {
            percentage = 0; // 음수가 되지 않도록 제한
        } else if (percentage > 100) {
            percentage = 100; // 100% 이상으로 가지 않도록 제한
        }

        // 10% 이상의 변화가 있을 때만 HTTP 요청 전송
        if (abs(percentage - lastPercentage) >= 10) {
            send_http_request(percentage);
            lastPercentage = percentage;
        }

        // 상태 출력
        printf("현재 남은 퍼센티지: %d%%\n", percentage);
        fflush(stdout);

        // 100ms 대기 후 다음 측정
        usleep(100000);
    }

    return 0;
}
