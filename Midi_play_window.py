# coding=gbk
import os
import tkinter as tk
import time
import threading


def main():
    playing = False
    wait_flag = False
    yinyu = "2"

    xy_bianji = 0.03
    xy_labelheight = 0.05
    xy_betweenlabel = (1 - 2 * xy_bianji) / 10 - xy_labelheight
    xy_widths = (1 - 3 * xy_bianji) / 2
    xy_buttonwidth = (xy_widths - xy_bianji) / 2

    root = tk.Tk()
    root.title("Midi piano player")
    root.geometry('800x500')

    var1 = tk.StringVar()
    var1.set("choose your muisc")
    la = tk.Label(root, textvariable=var1, font=('思源黑体 CN Light', 15), width=30, bg='white')
    la.place(relwidth=xy_widths, relheight=xy_labelheight, relx=(xy_widths + 2 * xy_bianji), rely=xy_bianji)

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
    lb.place(relwidth=xy_widths, relheight=(1 - 2 * xy_bianji), relx=xy_bianji, rely=xy_bianji)

    song_name = ""

    def wait(sec=0):
        nonlocal var1, wait_flag
        wait_flag = True
        time.sleep(sec)
        var1.set("now playing: " + song_name)
        wait_flag = False

    def play_s(exe_name, music_name):
        nonlocal var1, playing, yinyu
        playing = True
        f = os.popen(exe_name + " " + music_name + ".mid " + yinyu)
        f.close()
        playing = False
        var1.set("choose your muisc")

    def playm():
        nonlocal song_name, playing, wait_flag
        if not playing:
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
    bu_p.place(relx=(xy_widths + 2 * xy_bianji), rely=(1 - xy_bianji - xy_labelheight), relwidth=xy_buttonwidth, relheight=xy_labelheight)
    bu_s = tk.Button(root, text='stop', font=('Lato', 11), command=stop_playing)
    bu_s.place(relx=(3 * xy_bianji + xy_widths + xy_buttonwidth), rely=(1 - xy_bianji - xy_labelheight), relwidth=xy_buttonwidth, relheight=xy_labelheight)

    def kill_proc_exit(name='Midi2Py.exe'):
        nonlocal root
        stop_playing(name)
        root.destroy()

    frame_r = tk.Frame(root, bd=1, relief='solid')
    frame_r.place(relwidth=xy_widths, relheight=xy_labelheight, relx=(xy_bianji * 2 + xy_widths), rely=(xy_betweenlabel + xy_labelheight + xy_bianji))

    l_yinyu = tk.Label(frame_r, text="range of sound(2 for lower): ")
    l_yinyu.pack(side="left")

    var_yy = tk.StringVar()
    var_yy.set("2")

    def print_selection():
        nonlocal yinyu
        yinyu = var_yy.get()

    r1 = tk.Radiobutton(frame_r, text="1", variable=var_yy, value="1", command=print_selection)
    r1.pack(side="left")
    r2 = tk.Radiobutton(frame_r, text="2", variable=var_yy, value="2", command=print_selection)
    r2.pack(side="left")

    root.protocol('WM_DELETE_WINDOW', kill_proc_exit)
    root.resizable(0, 0)
    root.mainloop()


if __name__ == "__main__":
    main()
