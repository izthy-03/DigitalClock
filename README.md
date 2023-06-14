# DigitalClock
工科创Ⅱ-A大作业，2023-6-10



## 串口通信协议

```md
?
```

在串口返回指令帮助



```md
init clock
```

将时钟重置到 2000-1-1-00:00:00，不更改闹钟和计时器



```md
get time
```

在串口返回当前时间，格式：$Time:\  hh:mm:ss$



```md
get date
```

在串口返回当前日期，格式：$Date:\  yyyy-mm-dd$



```md
get alarm
```

在串口返回设置的闹钟时间及状态



```md
set time <hh:mm:ss>
```

设置时钟时间，并在串口返回设置成功与否，可用时间格式

$hh:mm:ss,\ hh-mm-ss,\ hh/mm/ss$



```md
set date <yyyy:mm:dd>
```

设置时钟日期，并在串口返回设置成功与否，可用日期格式

$yyyy-mm-dd,\ yyyy:mm:dd,\ yyyy/mm/dd$



```md
set time <hh:mm:ss>
```

设置闹钟时间，并在串口返回设置成功与否，可用时间格式

$hh:mm:ss,\ hh-mm-ss,\ hh/mm/ss$



```md
run time
```

数码管切换到时钟时间显示模式



```md
run date
```

数码管切换到时钟日期显示模式



```md
run cdown
```

数码管切换到计时器显示模式，并开始倒计时



```md
enable alarm
```

开启闹钟



```md
disable alarm
```

关闭闹钟



```md
enable cdown
```

开启倒计时



```md
disable cdown
```

暂停倒计时



#### ps:

若指令输入错误、不完整或是数值不合法，串口会返回相应的报错和提示信息



