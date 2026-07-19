# ProteusLite-Qt

پروژه درس برنامه‌سازی شیءگرا با زبان ++C و فریم‌ورک Qt است. هدف ما ساخت یک نرم‌افزار دسکتاپ برای طراحی مدار و شبیه‌سازی ساده رفتار قطعات الکترونیکی است. ظاهر کلی برنامه از Proteus الهام گرفته شده، اما ساختار پروژه با تمرکز بر مفاهیم شیءگرایی، جداسازی بخش‌های مختلف برنامه و توسعه مرحله‌به‌مرحله طراحی شده است.

## وضعیت فعلی پروژه

نسخه موجود در شاخه `develop` شامل بخش‌های پیاده‌سازی‌شده تا پایان بخش ششم است. در این نسخه کاربر می‌تواند پروژه جدید بسازد، قطعات را از کتابخانه انتخاب کند، آن‌ها را روی بوم قرار دهد، مشخصاتشان را تغییر دهد، بین پایه‌ها سیم بکشد و نتیجه ساده شبیه‌سازی را مشاهده کند.

قابلیت‌های فعلی پروژه:

- منوی آغازین برای ساخت یا بازکردن پروژه
- نگهداری فهرست پروژه‌های اخیر
- بوم طراحی با Grid، Snap، Zoom و Pan
- کتابخانه دسته‌بندی‌شده قطعات
- جست‌وجو و پیش‌نمایش قطعه
- قرار دادن و انتخاب قطعات روی بوم
- جابه‌جایی، چرخش، قرینه‌سازی و حذف قطعه
- ویرایش ویژگی‌های هر قطعه از پنجره Properties
- سیم‌کشی ۹۰ درجه و تشخیص پایه‌ها
- ساخت و مدیریت Junction
- مدل شیءگرای قطعات پایه
- منابع DC، باتری، Clock و Ground
- مقاومت، خازن و سلف
- Switch، Push Button، LED و Seven Segment
- گیت‌های منطقی و D Flip-Flop
- تشخیص ورودی شناور و درایورهای متناقض
- نمایش وضعیت مدار در پنل `Section 6 Monitor`

## ابزارها و فناوری‌ها

- C++17
- Qt Widgets
- Qt 5 یا Qt 6
- CMake 3.16 یا جدیدتر
- Git و GitHub

## ساخت و اجرای پروژه

### اجرا با Qt Creator

1. پوشه پروژه را در Qt Creator باز کنید.
2. فایل `CMakeLists.txt` را انتخاب کنید.
3. یک Kit سازگار با Qt 5 یا Qt 6 در نظر بگیرید.
4. CMake را اجرا کنید.
5. پروژه را Build و سپس Run کنید.

پروژه در محیط Qt 6 و MinGW نیز Build و اجرا شده است.

### اجرا از خط فرمان

در صورتی که Qt و CMake در مسیر سیستم تنظیم شده باشند، دستورات زیر قابل استفاده‌اند:

```bash
cmake -S . -B build
cmake --build build
```

فایل اجرایی پروژه با نام `ProteusPro` ساخته می‌شود.

## فایل‌های اصلی پروژه

```text
main.cpp
mainwindow.h / mainwindow.cpp
startmenudialog.h / startmenudialog.cpp
projectdocument.h / projectdocument.cpp
recentprojectsmanager.h / recentprojectsmanager.cpp
canvasview.h / canvasview.cpp
componentdefinition.h / componentdefinition.cpp
componentlibrarypanel.h / componentlibrarypanel.cpp
schematicpreviewwidget.h / schematicpreviewwidget.cpp
circuitmodel.h / circuitmodel.cpp
componentitem.h / componentitem.cpp
wireitem.h / wireitem.cpp
junctionitem.h / junctionitem.cpp
basiccomponent.h / basiccomponent.cpp
section05controller.h / section05controller.cpp
section06controller.h / section06controller.cpp
```

هر بخش مسئولیت مشخصی دارد. برای نمونه، `CanvasView` رفتار بوم را کنترل می‌کند، `ComponentLibraryPanel` کتابخانه قطعات را می‌سازد، `CircuitGraph` اطلاعات اتصال‌ها را نگه می‌دارد و کنترلرهای بخش پنجم و ششم رابط بین مدل مدار و محیط گرافیکی هستند.

## شاخه‌های پروژه

- `main`: شاخه نسخه پایدار
- `develop`: شاخه یکپارچه و نسخه فعلی توسعه
- `start-menu-project`: منوی آغازین و مدیریت پروژه
- `canvas-workspace`: بوم و محیط طراحی
- `component-library`: کتابخانه قطعات
- `feature/basic-components`: تعامل با قطعات و سیستم سیم‌کشی
- `feature/simulation-components`: قطعات پایه و منطق شبیه‌سازی

## مستندات بخش‌ها

توضیحات هر مرحله در فایل جداگانه قرار گرفته است:

- [بخش سوم: بوم و محیط طراحی](SECTION03_README_FA.md)
- [بخش چهارم: کتابخانه قطعات](SECTION04_README_FA.md)
- [بخش پنجم: تعامل با قطعات و سیم‌کشی](SECTION05_README_FA.md)
- [بخش ششم: قطعات پایه و شبیه‌سازی](SECTION06_README_FA.md)

## نکته پایانی

این مخزن یک پروژه دانشگاهی در حال توسعه است. در پیاده‌سازی آن تلاش شده منطق داده، رابط گرافیکی و کنترل رفتار برنامه تا جای ممکن از یکدیگر جدا باشند تا توسعه بخش‌های بعدی ساده‌تر و قابل مدیریت‌تر باشد.
