# StayHub

StayHub is a Qt/C++ desktop room-rental management system for tracking rooms, tenants, monthly/daily bills, utilities, payments, payment history, income, and Telegram bill notifications.

## Features

- Room and tenant management
- Monthly and short-term/daily stays
- Electricity and water utility tracking
- Monthly bill generation and payment recording
- Partial payments with **Paid / Partial / Unpaid** status tracking
- Payment status filter for quick tenant-bill tracking
- Payment history and income dashboard
- Tenant detail dashboard — double-click (or press Enter on) a room to see full tenant info, this month's live bill, and complete payment history in one view
- Telegram bill and payment-receipt notifications
- ABA KHQR payment image setup (Settings), optionally attached to bills/reminders sent on Telegram
- Login and account creation
- All editable input fields select their existing text automatically when focused
- User data is stored in the operating system's application-data folder

## Payment status

A bill is:

- **Paid** — the full bill amount has been received
- **Partial** — some money has been received, but a balance remains
- **Unpaid** — nothing has been received yet

The Payments page has a status filter plus a month filter.

## ABA KHQR

Owners can upload a screenshot of their ABA KHQR payment code in Settings. It's stored in the database (not just a file path), so it survives the original file being moved or deleted. When a bill or payment reminder is sent to a tenant on Telegram, StayHub asks each time whether to attach it.

## Requirements

- Qt 6.x
- C++17 compiler
- Qt Widgets, SQL, Network, and Multimedia modules
- SQLite Qt SQL driver

## Build with Qt Creator

1. Open `StayHub.pro` in Qt Creator.
2. Select a Qt 6 kit with a C++ compiler.
3. Build the project.
4. Run `StayHub`.

The SQLite database is created automatically in the OS-specific application-data directory.

## Build from a Qt command prompt

```text
qmake StayHub.pro CONFIG+=release
mingw32-make release -j4
```

For a Windows release, run `windeployqt` on the generated `StayHub.exe` and distribute the executable together with the deployed Qt DLLs/plugins.

## GitHub release

A GitHub repository can contain the source code from this folder. For a downloadable Windows release, build the project with a Windows Qt kit and attach a ZIP containing the deployed `StayHub.exe` and its required Qt files to a GitHub Release.

The included GitHub Actions workflow can automate a Windows build when a version tag such as `v1.0.0` is pushed.
