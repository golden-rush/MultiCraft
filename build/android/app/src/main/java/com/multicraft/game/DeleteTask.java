package com.easycraft.game;

import android.os.AsyncTask;
import android.view.View;

import com.bugsnag.android.Bugsnag;

import org.apache.commons.io.FileUtils;

import java.io.File;
import java.io.IOException;

class DeleteTask extends AsyncTask<String, Void, Void> {

    private CallBackListener listener;
    private String location;

    @Override
    protected void onPreExecute() {
        super.onPreExecute();
        listener.updateViews(R.string.rm_old, View.VISIBLE, View.VISIBLE);
    }

    @Override
    protected Void doInBackground(String... params) {
        location = params[0];
        for (String p : params)
            deleteFiles(p);
        return null;
    }


    @Override
    protected void onPostExecute(Void result) {
        listener.onEvent("DeleteTask", location);
    }

    private void deleteFiles(String path) {
        File file = new File(path);
        if (file.exists()) {
            try {
                if (file.isDirectory())
                    FileUtils.deleteDirectory(file);
                else FileUtils.deleteQuietly(file);
            } catch (IOException e) {
                Bugsnag.notify(e);
            }
        }
    }

    void setListener(CallBackListener listener) {
        this.listener = listener;
    }
}
