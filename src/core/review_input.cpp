#include "review_input.h"

review_action_t review_action(ui_view_state_t view, input_event_t evt) {
    switch (view) {
        case VIEW_CARD_FRONT:
            if (evt == EVT_DIAL_UP || evt == EVT_DIAL_CONFIRM) return REVIEW_SHOW_BACK;
            if (evt == EVT_DIAL_DOWN) return REVIEW_REMEMBERED;
            return REVIEW_NONE;
        case VIEW_CARD_BACK:
            if (evt == EVT_DIAL_UP) return REVIEW_FORGOT;
            if (evt == EVT_DIAL_CONFIRM) return REVIEW_SHOW_FRONT;
            if (evt == EVT_DIAL_DOWN) return REVIEW_REMEMBERED;
            return REVIEW_NONE;
        case VIEW_DONE_STATE:
            if (evt == EVT_DIAL_UP || evt == EVT_DIAL_CONFIRM || evt == EVT_DIAL_DOWN) return REVIEW_RESTART;
            return REVIEW_NONE;
        default:
            return REVIEW_NONE;
    }
}
