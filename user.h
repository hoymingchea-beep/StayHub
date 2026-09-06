#pragma once
#include <QString>

struct User {
    int id = -1;
    QString username;
    QString passwordHash;
    QString salt;
};
