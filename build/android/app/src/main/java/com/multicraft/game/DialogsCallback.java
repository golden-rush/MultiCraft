package com.easycraft.game;

interface DialogsCallback {
    void onPositive(String source);

    void onNegative(String source);

    void onNeutral(String source);
}
