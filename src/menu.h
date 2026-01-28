#ifndef MENU_H
#define MENU_H

#include <TFT_eSPI.h>

// 定义菜单项类型枚举
enum MenuItemType {
    MENU_TYPE_NORMAL,        // 普通菜单项
    MENU_TYPE_SUBMENU,       // 子菜单项
    MENU_TYPE_EDITABLE_BOOL, // 可编辑布尔值项
    MENU_TYPE_EDITABLE_TEXT, // 可编辑文本项
    MENU_TYPE_EDITABLE_NUMBER, // 可编辑数字项
    MENU_TYPE_ACTION         // 动作项
};

// 数字范围结构体，用于限制可编辑数字项的取值范围
struct NumberRange {
    int* value;      // 指向实际整数值的指针
    int minValue;    // 最小值
    int maxValue;    // 最大值
};

// 菜单项结构体定义
struct MenuItem {
    String label;              // 菜单项标签
    int id;                   // 菜单项ID
    bool enabled;             // 是否可用
    MenuItemType type;        // 菜单项类型
    void* value;              // 存储可编辑值的指针（根据类型决定实际指向的内容）
    struct Menu* submenu;     // 子菜单指针
    void (*action)();         // 动作函数指针
    
    // 构造函数：创建一个新的菜单项
    MenuItem(String l, int i, bool e = true) : label(l), id(i), enabled(e) {
        type = MENU_TYPE_NORMAL;
        value = nullptr;
        submenu = nullptr;
        action = nullptr;
    }
};

/**
 * Menu类 - 用于创建和管理多级菜单系统
 * 支持多种菜单项类型：普通项、子菜单、可编辑项（布尔、文本、数字）、动作项
 * 使用TFT_eSPI库进行显示，并通过Sprite优化绘图性能
 */
class Menu {
private:
    TFT_eSPI& tft;           // 引用TFT显示屏对象
    MenuItem** items;        // 菜单项数组指针
    int itemCount;           // 当前菜单项总数
    int selected;            // 当前选中的菜单项索引
    int topItem;             // 显示的第一个菜单项索引（用于滚动）
    int maxVisibleItems;     // 屏幕上可显示的最大菜单项数
    int itemHeight;          // 每个菜单项的高度
    int itemWidth;           // 菜单项宽度
    TFT_eSprite* sprite;     // 用于绘制的Sprite对象，提高绘图效率
    bool editingMode;        // 编辑模式标志（true表示正在编辑可编辑项）
    int menuId;              // 菜单唯一标识符
    Menu* parentMenu;        // 父菜单指针（用于返回上级菜单）
    
    // 绘制单个菜单项
    void drawMenuItem(int index, bool isSelected);
    
    // 清除菜单显示区域
    void clearMenuArea();
    
public:
    // 构造函数：初始化菜单对象
    Menu(TFT_eSPI& display);
    
    // 析构函数：释放分配的内存资源
    ~Menu();
    
    // 添加菜单项到当前菜单
    void addItem(MenuItem* item);
    
    // 初始化菜单（创建Sprite、设置显示参数等）
    void init();
    
    // 显示菜单（绘制并推送到屏幕）
    void show();
    
    // 处理菜单导航 - 向上移动选择
    void navigateUp();
    
    // 处理菜单导航 - 向下移动选择
    void navigateDown();
    
    // 选择当前高亮的菜单项（执行相应操作）
    int selectCurrent();
    
    // 获取当前选中的菜单项ID
    int getCurrentSelection();
    
    // 更新菜单显示
    void updateDisplay();
    
    // 设置屏幕尺寸参数
    void setDimensions(int width, int height);
    
    // 添加子菜单项（带有子菜单的菜单项）
    void addSubmenuItem(const char* label, Menu* submenu);
    
    // 添加布尔型可编辑项（如开关：开/关）
    void addEditableBoolItem(const char* label, bool* value);
    
    // 添加文本型可编辑项（如字符串输入）
    void addEditableTextItem(const char* label, char* value);
    
    // 添加数字型可编辑项（带范围限制的数字输入）
    void addEditableNumberItem(const char* label, int* value, int minValue, int maxValue);
    
    // 添加动作项（点击后执行特定函数的菜单项）
    void addActionItem(const char* label, void (*action)());
    
    // 退出编辑模式
    void exitEditingMode();
    
    // 检查是否处于编辑模式
    bool isInEditingMode();
    
    // 设置父菜单（用于菜单层级导航）
    void setParent(Menu* parent);
    
    // 获取父菜单
    Menu* getParent();
    
    // 获取菜单ID
    int getId();
    
    // 设置菜单ID
    void setId(int id);
    
    // 获取当前选中项的标签
    String getCurrentSelectionLabel();
};

#endif