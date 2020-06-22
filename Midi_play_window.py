# coding=gbk
import os
import tkinter as tk
import time
import threading


def main():
    playing = False
    wait_flag = False

    root = tk.Tk()
    root.title("Midi piano player")
    root.geometry('500x700')

    var1 = tk.StringVar()
    var1.set("choose your muisc")
    la = tk.Label(root, textvariable=var1, font=('Lato', 15))
    la.pack()

    exe_name = "Midi2Py.exe"
    midi_file_name = "midi_try"
    f = os.popen("dir " + midi_file_name)
    list_f = f.read().split(" ")
    count = 0
    index2name = []
    for i in list_f:
        if ".mid" in i:
            count += 1
            tmp = i.split(".mid")[0]
            index2name.append("%2d." % count + tmp)
    f.close()

    lb = tk.Listbox(root, width=45, height=30)
    for i in index2name:
        lb.insert('end', i)
    lb.pack()

    song_name = ""

    def wait(sec=0):
        nonlocal var1, wait_flag
        wait_flag = True
        time.sleep(sec)
        var1.set("now playing: " + song_name)
        wait_flag = False

    def play_s(exe_name, music_name):
        nonlocal var1, playing
        playing = True
        f = os.popen(exe_name + " " + music_name + ".mid")
        f.close()
        playing = False
        var1.set("choose your muisc")

    def playm():
        nonlocal song_name, playing, wait_flag
        if not playing:
            # os.chdir(path + "\\")
            song_name = lb.get(lb.curselection()).split('.')[1]
            var1.set("now playing: " + song_name)
            t_play = threading.Thread(target=play_s, args=(exe_name, song_name))
            t_play.setDaemon(True)
            t_play.start()
        else:
            if not wait_flag:
                var1.set("please wait")
                t1 = threading.Timer(3, wait)
                t1.start()

    def stop_playing(name='Midi2Py.exe'):
        nonlocal playing
        if not playing:
            pass
        else:
            tmp = os.popen("taskkill /f /t /im {}".format(name))
            tmp.close()

    bu_p = tk.Button(root, text='play', font=('Lato', 11), command=playm)
    bu_p.place(relx=0.25, rely=0.85, relwidth=0.2)
    bu_s = tk.Button(root, text='stop', font=('Lato', 11), command=stop_playing)
    bu_s.place(relx=0.55, rely=0.85, relwidth=0.2)

    def kill_proc_exit(name='Midi2Py.exe'):
        nonlocal root
        stop_playing(name)
        root.destroy()

    root.protocol('WM_DELETE_WINDOW', kill_proc_exit)
    root.resizable(0, 0)
    root.mainloop()


if __name__ == "__main__":
    main()
