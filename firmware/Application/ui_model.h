/** UI 页面、参数编辑和显示排版，不依赖 HAL/FreeRTOS，便于主机验证。 */
#ifndef UI_MODEL_H
#define UI_MODEL_H
#include "state_machine.h"
#include "fault_record.h"

#define UI_LINE_COUNT 8U
#define UI_LINE_SIZE 22U /* 128 像素屏每行最多 21 个 6x8 字符。 */
typedef enum { UI_PAGE_MAIN, UI_PAGE_SENSORS, UI_PAGE_ENVIRONMENT,
               UI_PAGE_SETTINGS, UI_PAGE_FAULTS, UI_PAGE_COUNT } UI_Page_t;
typedef enum { UI_ACTION_POWER, UI_ACTION_MODE, UI_ACTION_UP, UI_ACTION_DOWN,
               UI_ACTION_ROTATE, UI_ACTION_PAGE, UI_ACTION_RESET, UI_ACTION_STOP } UI_Action_t;
typedef enum { UI_FEEDBACK_CLICK, UI_FEEDBACK_MODE, UI_FEEDBACK_WARNING } UI_Feedback_t;
typedef struct {
    UI_Page_t page;
    uint8_t field, fault_index;
    bool dirty;
    SystemConfig_t draft;
} UI_Model_t;
typedef struct {
    UI_Event_t input;
    bool save_config;
    UI_Feedback_t feedback;
} UI_Result_t;
typedef struct {
    System_Status_t status;
    SensorData_t sensor;
    uint32_t now_ms, rpm, motor_faults;
    uint16_t duty;
    bool speed_valid, motor_enabled, config_saved, last_fault_valid;
    FaultRecord_t last_fault;
} UI_View_t;
void UI_Model_Init(UI_Model_t *ui, const SystemConfig_t *config);
/* current 是运行配置快照；只有进入设置页才据此重建草稿。 */
UI_Result_t UI_Model_Action(UI_Model_t *ui, UI_Action_t action, int32_t delta,
                           const SystemConfig_t *current, const System_Status_t *status);
void UI_Model_Render(const UI_Model_t *ui, const UI_View_t *view,
                     char lines[UI_LINE_COUNT][UI_LINE_SIZE]);
#endif
