#include "menu.h"
#include <TFT_eSPI.h>

/**
 * Menu构造函数
 * 初始化菜单的基本属性和参数
 * @param display - TFT显示屏对象引用
 */
Menu::Menu(TFT_eSPI& display) : tft(display) {
    items = nullptr;                    // 菜单项数组初始为空
    itemCount = 0;                      // 初始菜单项计数为0
    selected = 0;                       // 初始选中第一项
    topItem = 0;                        // 初始显示从第一项开始
    itemHeight = 30;                    // 默认每项高度为30像素
    maxVisibleItems = tft.height() / itemHeight;  // 根据屏幕高度计算最大可见项数
    itemWidth = tft.width();            // 菜单项宽度等于屏幕宽度
    sprite = new TFT_eSprite(&tft);     // 创建精灵对象用于高效绘图
    editingMode = false;                // 初始不在编辑模式
    parentMenu = nullptr;               // 初始无父菜单
    menuId = -1;                        // 初始菜单ID为-1（无效）
}

/**
 * Menu析构函数
 * 释放分配的内存资源，防止内存泄漏
 */
Menu::~Menu() {
    if (items) {
        for (int i = 0; i < itemCount; i++) {
            // 释放NumberRange结构体内存（如果是数字类型的话）
            if (items[i]->type == MENU_TYPE_EDITABLE_NUMBER) {
                delete (NumberRange*)items[i]->value;
            }
            delete items[i];  // 删除每个菜单项及其内容
        }
        delete[] items;
    }
    if (sprite) {
        sprite->deleteSprite();  // 删除精灵对象
        delete sprite;
    }
}

/**
 * 添加菜单项到当前菜单
 * @param item - 要添加的菜单项指针
 */
void Menu::addItem(MenuItem* item) {
    MenuItem** newItems = new MenuItem*[itemCount + 1];
    
    // 复制现有项目
    for (int i = 0; i < itemCount; i++) {
        newItems[i] = items[i];
    }
    
    // 添加新项目
    newItems[itemCount] = item;
    itemCount++;
    
    // 释放旧数组并更新指针
    if (items) {
        delete[] items;
    }
    items = newItems;
}

/**
 * 初始化菜单
 * 创建精灵对象并设置基本显示参数
 */
void Menu::init() {
    // 初始化Sprite
    sprite->createSprite(itemWidth, itemHeight * maxVisibleItems);
    
    // 设置背景色
    sprite->fillSprite(TFT_BLACK);
    
    // 设置字体颜色
    sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    
    // 设置字体大小
    sprite->setTextSize(2);
}

/**
 * 显示菜单
 * 清除屏幕并绘制当前可见的菜单项
 */
void Menu::show() {
    clearMenuArea();
    
    // 计算实际可显示的菜单项数量
    int visibleCount = min(maxVisibleItems, itemCount);
    
    // 绘制可见的菜单项
    for (int i = 0; i < visibleCount; i++) {
        int itemIndex = topItem + i;
        if (itemIndex < itemCount) {
            bool isSelected = (selected == itemIndex);
            drawMenuItem(itemIndex, isSelected);
        }
    }
    
    // 将Sprite显示到屏幕上
    sprite->pushSprite(0, 0);
}

/**
 * 绘制单个菜单项
 * @param index - 菜单项索引
 * @param isSelected - 是否被选中
 */
void Menu::drawMenuItem(int index, bool isSelected) {
    int yPos = (index - topItem) * itemHeight;
    
    // 绘制选中项的背景
    if (isSelected) {
        sprite->fillRect(0, yPos, itemWidth, itemHeight, TFT_BLUE);
        sprite->setTextColor(TFT_WHITE, TFT_BLUE);
    } else {
        sprite->fillRect(0, yPos, itemWidth, itemHeight, TFT_BLACK);
        sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    }
    
    // 检查菜单项类型并相应处理
    MenuItem* item = items[index];
    String displayText = item->label;
    
    switch (item->type) {
        case MENU_TYPE_SUBMENU:
            displayText += " >>";  // 子菜单项显示箭头指示
            break;
        case MENU_TYPE_EDITABLE_BOOL:
            displayText += ": " + String(*(bool*)item->value ? "ON" : "OFF");  // 显示布尔值状态
            break;
        case MENU_TYPE_EDITABLE_TEXT:
            displayText += ": " + String((char*)item->value);  // 显示文本值
            break;
        case MENU_TYPE_EDITABLE_NUMBER:
        {
            NumberRange* range = (NumberRange*)item->value;
            displayText += ": " + String(*range->value);  // 显示数字值
            break;
        }
        default:
            break;
    }

    // 如果菜单项不可用，显示灰色文字
    if (!item->enabled) {
        sprite->setTextColor(TFT_DARKGREEN, isSelected ? TFT_BLUE : TFT_BLACK);
    }
    
    // 绘制菜单项文本
    sprite->drawString(displayText, 10, yPos + (itemHeight - 16) / 2, 2); // 垂直居中
    
    // 恢复默认文本颜色
    if (isSelected) {
        sprite->setTextColor(TFT_WHITE, TFT_BLUE);
    } else {
        sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    }
}

/**
 * 清除菜单显示区域
 * 用黑色填充精灵对象
 */
void Menu::clearMenuArea() {
    sprite->fillSprite(TFT_BLACK);
}

/**
 * 向上导航菜单
 * 在非编辑模式下移动选中项，编辑模式下修改数值
 */
void Menu::navigateUp() {
    if (editingMode) {
        // 在编辑模式下，改变数值
        MenuItem* currentItem = items[selected];
        if (currentItem->type == MENU_TYPE_EDITABLE_BOOL) {
            *(bool*)currentItem->value = !*(bool*)currentItem->value;  // 切换布尔值
        } else if (currentItem->type == MENU_TYPE_EDITABLE_NUMBER) {
            NumberRange* range = (NumberRange*)currentItem->value;
            if (*range->value < range->maxValue) {
                (*range->value)++;  // 增加数字值，但不超过最大值
            }
        }
    } else {
        // 正常导航模式
        if (selected > 0) {
            selected--;
            
            // 如果当前选中项移出了可见区域顶部，调整显示区域
            if (selected < topItem) {
                topItem = selected;
            }
        }
    }
    
    updateDisplay();
}

/**
 * 向下导航菜单
 * 在非编辑模式下移动选中项，编辑模式下修改数值
 */
void Menu::navigateDown() {
    if (editingMode) {
        // 在编辑模式下，改变数值
        MenuItem* currentItem = items[selected];
        if (currentItem->type == MENU_TYPE_EDITABLE_BOOL) {
            *(bool*)currentItem->value = !*(bool*)currentItem->value;  // 切换布尔值
        } else if (currentItem->type == MENU_TYPE_EDITABLE_NUMBER) {
            NumberRange* range = (NumberRange*)currentItem->value;
            if (*range->value > range->minValue) {
                (*range->value)--;  // 减少数值，但不低于最小值
            }
        }
    } else {
        // 正常导航模式
        if (selected < itemCount - 1) {
            selected++;
            
            // 如果当前选中项移出了可见区域底部，调整显示区域
            if (selected >= topItem + maxVisibleItems) {
                topItem = selected - maxVisibleItems + 1;
            }
        }
    }
    
    updateDisplay();
}

/**
 * 选择当前高亮的菜单项
 * 根据菜单项类型执行相应的操作
 * @return 子菜单ID或-1（如果无效选择）
 */
int Menu::selectCurrent() {
    if (selected >= 0 && selected < itemCount) {
        MenuItem* item = items[selected];
        
        // 根据菜单项类型执行不同的操作
        switch (item->type) {
            case MENU_TYPE_SUBMENU:
                // 进入子菜单
                if (item->submenu) {
                    return item->submenu->getId();  // 返回子菜单ID
                }
                break;
                
            case MENU_TYPE_EDITABLE_BOOL:
            case MENU_TYPE_EDITABLE_TEXT:
            case MENU_TYPE_EDITABLE_NUMBER:
                // 开始编辑模式
                editingMode = !editingMode;
                break;
                
            case MENU_TYPE_ACTION:
                // 执行动作
                if (item->action) {
                    item->action();
                }
                break;
                
            default:
                break;
        }
    }
    return -1;  // 表示无效选择
}

/**
 * 获取当前选中的菜单项索引
 * @return 选中项的索引
 */
int Menu::getCurrentSelection() {
    return selected;
}

/**
 * 更新菜单显示
 * 重新绘制菜单界面
 */
void Menu::updateDisplay() {
    show();
}

/**
 * 设置屏幕尺寸参数
 * 用于适配不同分辨率的屏幕
 * @param width - 屏幕宽度
 * @param height - 屏幕高度
 */
void Menu::setDimensions(int width, int height) {
    itemWidth = width;
    maxVisibleItems = height / itemHeight;
    
    // 重新创建Sprite以适应新的尺寸
    if (sprite) {
        sprite->deleteSprite();
        sprite->createSprite(itemWidth, itemHeight * maxVisibleItems);
    }
}

/**
 * 添加子菜单项
 * @param label - 菜单项标签
 * @param submenu - 子菜单指针
 */
void Menu::addSubmenuItem(const char* label, Menu* submenu) {
    MenuItem* item = new MenuItem(label, itemCount, true);  // 使用构造函数创建菜单项
    item->type = MENU_TYPE_SUBMENU;
    item->submenu = submenu;
    if(submenu != nullptr) {
        submenu->setParent(this);  // 设置子菜单的父菜单引用
    }
    addItem(item);
}

/**
 * 添加布尔型可编辑项
 * @param label - 菜单项标签
 * @param value - 指向布尔值的指针
 */
void Menu::addEditableBoolItem(const char* label, bool* value) {
    MenuItem* item = new MenuItem(label, itemCount, true);  // 使用构造函数创建菜单项
    item->type = MENU_TYPE_EDITABLE_BOOL;
    item->value = (void*)value;
    addItem(item);
}

/**
 * 添加文本型可编辑项
 * @param label - 菜单项标签
 * @param value - 指向字符数组的指针
 */
void Menu::addEditableTextItem(const char* label, char* value) {
    MenuItem* item = new MenuItem(label, itemCount, true);  // 使用构造函数创建菜单项
    item->type = MENU_TYPE_EDITABLE_TEXT;
    item->value = (void*)value;
    addItem(item);
}

/**
 * 添加数字型可编辑项
 * @param label - 菜单项标签
 * @param value - 指向整数值的指针
 * @param minValue - 最小值
 * @param maxValue - 最大值
 */
void Menu::addEditableNumberItem(const char* label, int* value, int minValue, int maxValue) {
    NumberRange* range = new NumberRange();
    range->value = value;
    range->minValue = minValue;
    range->maxValue = maxValue;
    
    MenuItem* item = new MenuItem(label, itemCount, true);  // 使用构造函数创建菜单项
    item->label = label;
    item->type = MENU_TYPE_EDITABLE_NUMBER;
    item->value = (void*)range;
    addItem(item);
}

/**
 * 添加动作项
 * @param label - 菜单项标签
 * @param action - 动作函数指针
 */
void Menu::addActionItem(const char* label, void (*action)()) {
    MenuItem* item = new MenuItem(label, itemCount, true);  // 使用构造函数创建菜单项
    item->type = MENU_TYPE_ACTION;
    item->action = action;
    addItem(item);
}

/**
 * 退出编辑模式
 */
void Menu::exitEditingMode() {
    editingMode = false;
}

/**
 * 检查是否处于编辑模式
 * @return 编辑模式状态
 */
bool Menu::isInEditingMode() {
    return editingMode;
}

/**
 * 设置父菜单
 * @param parent - 父菜单指针
 */
void Menu::setParent(Menu* parent) {
    parentMenu = parent;
}

/**
 * 获取父菜单
 * @return 父菜单指针
 */
Menu* Menu::getParent() {
    return parentMenu;
}

/**
 * 获取菜单ID
 * @return 菜单ID
 */
int Menu::getId() {
    return menuId;
}

/**
 * 设置菜单ID
 * @param id - 菜单ID
 */
void Menu::setId(int id) {
    menuId = id;
}

/**
 * 获取当前选中项的标签
 * @return 当前选中菜单项的标签
 */
String Menu::getCurrentSelectionLabel() {
    if (selected >= 0 && selected < itemCount) {
        return items[selected]->label;
    }
    return "";  // 返回空字符串如果索引无效
}