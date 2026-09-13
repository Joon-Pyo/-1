/*
 * 커스텀 Yacht(요트) 주사위 게임 - C 언어 구현 (v2)
 *
 * ─────────────────────────────────────────────────────────
 *  "한 판"의 정의
 *  ─────────────────────────────────────────────────────────
 *  한 판에는 아래 6개의 점수 칸이 있다.
 *      원페어 / 트리플 / 포카드 / 풀하우스 / 스트레이트 / 요트
 *
 *  주사위 5개를 굴릴 때마다:
 *    1) 이번에 나온 눈으로 성립하는 족보들을 모두 찾는다.
 *    2) 그 중 "아직 채워지지 않은 칸" 중에서 점수가 가장 높은
 *       족보를 골라 그 칸에 점수를 기입한다.
 *    3) 성립하는 족보가 하나도 없거나, 성립하는 족보 칸이
 *       전부 이미 채워져 있다면 -> 기댓값이 가장 낮은 순서
 *       (요트 > 포카드 > 풀하우스 > 트리플 > 스트레이트 > 원페어)
 *       대로 아직 비어있는 칸에 0점을 기입한다.
 *  4) 6개 칸이 모두 채워지면 그 판은 종료되고, 칸들의 점수를
 *     합산해 "판 점수"로 기록한다. (한 판 = 정확히 6번 굴림)
 *
 *  이 과정을 3판 반복하여 총점을 누적한다.
 *
 * ─────────────────────────────────────────────────────────
 *  점수 규칙
 *  ─────────────────────────────────────────────────────────
 *   - 원페어  : 같은 눈이 정확히 2개 -> 눈값 * 2 점
 *   - 트리플  : 같은 눈이 정확히 3개 -> 눈값 * 3 점
 *   - 포카드  : 같은 눈이 정확히 4개 -> 눈값 * 4 점
 *   - 풀하우스: 3개 + 2개 조합       -> 25 점 (고정)
 *   - 스트레이트: 연속된 눈이 4개 이상 -> 30 점 (고정)
 *   - 요트    : 같은 눈이 5개 전부   -> 50 점 (고정)
 *
 *  여러 족보가 동시에 성립하면(그리고 둘 다 비어있는 칸이면)
 *  점수가 더 높은 쪽 칸에 기입한다.
 *
 * ─────────────────────────────────────────────────────────
 *  필수 기능 매핑
 *  ─────────────────────────────────────────────────────────
 *   1) 주사위 5개를 무작위로 굴려 눈을 출력            -> roll_dice()
 *   2) 나온 조합의 족보를 판정하고 점수를 계산          -> compute_candidates()
 *   3) 3판을 반복하여 총점을 누적 계산                  -> main()의 판 루프
 *   4) 총점과 족보 판별 기록을 출력                     -> 매 굴림 로그 + 판별 결과표
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define NUM_DICE    5
#define NUM_ROUNDS  3
#define CAT_COUNT   6
#define NAME_LEN    32

typedef enum {
    CAT_PAIR = 0,
    CAT_TRIPLE,
    CAT_FOUR,
    CAT_FULLHOUSE,
    CAT_STRAIGHT,
    CAT_YACHT
} Category;

/* 점수 칸 이름 */
static const char *CATEGORY_NAME[CAT_COUNT] = {
    "원페어", "트리플", "포카드", "풀하우스", "스트레이트", "요트"
};

/* 어떤 족보도 성립하지 않을 때, 기댓값(확률 x 점수)이 낮은 순으로
   0점을 채울 우선순위. (실제 5주사위 확률로 계산한 결과)
     요트    : 6/7776  x 50 ≈ 0.039
     포카드  : 150/7776 x 14 ≈ 0.27   (14 = 눈값 평균 3.5 * 4)
     풀하우스: 300/7776 x 25 ≈ 0.96
     트리플  : 1500/7776 x 10.5 ≈ 2.03
     스트레이트: 1200/7776 x 30 ≈ 4.63
     원페어  : 5700/7776 x 7   ≈ 5.13   (가장 높음 -> 맨 마지막에 0점 처리) */
static const Category ZERO_FILL_PRIORITY[CAT_COUNT] = {
    CAT_YACHT, CAT_FOUR, CAT_FULLHOUSE, CAT_TRIPLE, CAT_STRAIGHT, CAT_PAIR
};

/* 주사위 5개를 굴려서 dice 배열에 저장 */
static void roll_dice(int dice[NUM_DICE]) {
    for (int i = 0; i < NUM_DICE; i++) {
        dice[i] = (rand() % 6) + 1; /* 1~6 */
    }
}

/* 주사위 배열을 "3 3 5 5 6" 형태 문자열로 출력 */
static void print_dice(const int dice[NUM_DICE]) {
    for (int i = 0; i < NUM_DICE; i++) {
        printf("%d", dice[i]);
        if (i != NUM_DICE - 1) printf(" ");
    }
}

/*
 * 이번 굴림에서 각 족보가 성립하는지, 성립한다면 몇 점인지 계산한다.
 * candidate_score[cat] : 성립하면 해당 점수, 아니면 0
 * candidate_value[cat] : 원페어/트리플/포카드일 때 어떤 눈으로 성립했는지 (표시용)
 */
static void compute_candidates(const int dice[NUM_DICE],
                                int candidate_score[CAT_COUNT],
                                int candidate_value[CAT_COUNT]) {
    memset(candidate_score, 0, sizeof(int) * CAT_COUNT);
    memset(candidate_value, 0, sizeof(int) * CAT_COUNT);

    int cnt[7] = {0};
    for (int i = 0; i < NUM_DICE; i++) cnt[dice[i]]++;

    int triple_val = 0; /* 풀하우스 판정에 재사용 */

    for (int v = 1; v <= 6; v++) {
        if (cnt[v] == 2) {
            int s = v * 2;
            if (s > candidate_score[CAT_PAIR]) { /* 투페어면 높은 값 채택 */
                candidate_score[CAT_PAIR] = s;
                candidate_value[CAT_PAIR] = v;
            }
        }
        if (cnt[v] == 3) {
            candidate_score[CAT_TRIPLE] = v * 3;
            candidate_value[CAT_TRIPLE] = v;
            triple_val = v;
        }
        if (cnt[v] == 4) {
            candidate_score[CAT_FOUR] = v * 4;
            candidate_value[CAT_FOUR] = v;
        }
        if (cnt[v] == 5) {
            candidate_score[CAT_YACHT] = 50;
        }
    }

    /* 풀하우스 : 3개짜리와 다른 값의 2개짜리가 함께 있는지 */
    if (triple_val != 0) {
        for (int v = 1; v <= 6; v++) {
            if (v != triple_val && cnt[v] == 2) {
                candidate_score[CAT_FULLHOUSE] = 25;
                break;
            }
        }
    }

    /* 스트레이트 : 서로 다른 눈들 중 연속된 값이 4개 이상 */
    {
        int present[7] = {0};
        for (int i = 0; i < NUM_DICE; i++) present[dice[i]] = 1;
        int run = 0, max_run = 0;
        for (int v = 1; v <= 6; v++) {
            if (present[v]) { run++; if (run > max_run) max_run = run; }
            else run = 0;
        }
        if (max_run >= 4) candidate_score[CAT_STRAIGHT] = 30;
    }
}

/* 족보 이름을 값과 함께 표시용 문자열로 만든다. 예) "원페어(4)" / "풀하우스" */
static void format_label(Category cat, int value, bool has_value, char out[NAME_LEN]) {
    if (has_value && (cat == CAT_PAIR || cat == CAT_TRIPLE || cat == CAT_FOUR)) {
        snprintf(out, NAME_LEN, "%s(%d)", CATEGORY_NAME[cat], value);
    } else {
        snprintf(out, NAME_LEN, "%s", CATEGORY_NAME[cat]);
    }
}

int main(void) {
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid() ^ (unsigned int)clock());

    int grand_total = 0;
    int round_totals[NUM_ROUNDS];

    printf("=============================================\n");
    printf("   커스텀 Yacht 주사위 게임 (%d판, 판당 6칸)\n", NUM_ROUNDS);
    printf("=============================================\n");

    for (int round = 0; round < NUM_ROUNDS; round++) {
        bool filled[CAT_COUNT] = { false, false, false, false, false, false };
        int slot_score[CAT_COUNT] = {0};
        char slot_label[CAT_COUNT][NAME_LEN];
        int roll_no = 0;

        printf("\n########## %d판 시작 ##########\n", round + 1);

        int filled_count = 0;
        while (filled_count < CAT_COUNT) {
            int dice[NUM_DICE];
            int candidate_score[CAT_COUNT];
            int candidate_value[CAT_COUNT];

            roll_dice(dice);
            roll_no++;
            compute_candidates(dice, candidate_score, candidate_value);

            /* 비어있는 칸들 중 가장 점수가 높은 족보 선택 */
            int best_cat = -1;
            int best_score = -1;
            for (int c = 0; c < CAT_COUNT; c++) {
                if (!filled[c] && candidate_score[c] > 0 && candidate_score[c] > best_score) {
                    best_score = candidate_score[c];
                    best_cat = c;
                }
            }

            bool forced_zero = false;
            if (best_cat == -1) {
                /* 성립하는 족보가 없거나 이미 다 채워진 칸뿐 -> 기댓값 낮은 순으로 0점 처리 */
                for (int i = 0; i < CAT_COUNT; i++) {
                    Category c = ZERO_FILL_PRIORITY[i];
                    if (!filled[c]) { best_cat = c; best_score = 0; forced_zero = true; break; }
                }
            }

            filled[best_cat] = true;
            slot_score[best_cat] = best_score;
            filled_count++;

            char label[NAME_LEN];
            if (forced_zero) {
                snprintf(label, NAME_LEN, "%s(0점 처리)", CATEGORY_NAME[best_cat]);
            } else {
                format_label((Category)best_cat, candidate_value[best_cat], true, label);
            }
            strncpy(slot_label[best_cat], label, NAME_LEN - 1);
            slot_label[best_cat][NAME_LEN - 1] = '\0';

            printf("[%d판 - %d번째 굴림] 주사위: ", round + 1, roll_no);
            print_dice(dice);
            printf("  ->  %s 칸에 %d점 기입\n", label, best_score);
        }

        int round_total = 0;
        for (int c = 0; c < CAT_COUNT; c++) round_total += slot_score[c];
        round_totals[round] = round_total;
        grand_total += round_total;

        printf("\n--- %d판 점수표 ---\n", round + 1);
        for (int c = 0; c < CAT_COUNT; c++) {
            printf("  %s : %d점\n", slot_label[c], slot_score[c]);
        }
        printf("  %d판 총점 (%d번 굴림) : %d점\n", round + 1, roll_no, round_total);
    }

    printf("\n=============================================\n");
    printf("                 최종 결과\n");
    printf("=============================================\n");
    for (int round = 0; round < NUM_ROUNDS; round++) {
        printf("  %d판 점수 : %d점\n", round + 1, round_totals[round]);
    }
    printf("---------------------------------------------\n");
    printf("  총 누적 점수 : %d점\n", grand_total);

    return 0;
}