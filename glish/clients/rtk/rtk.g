#rtk := client('rtk',suspend=T)
rtk := client('rtk')

func frame ( parent=F, relief='flat', borderwidth=2, side='top', padx=0, pady=0,
	     expand='both', background='lightgrey', width=70, height=50, cursor='',
	     title='glish/tk', icon='', newcmap=F, tlead=F, tpos='sw' )
	rtk->frame( parent, relief, side, borderwidth, padx, pady, expand, background, width, height, cursor, title, icon, newcmap, tlead, tpos )

func button ( parent, text='button', type='plain', padx=7, pady=3, width=0, height=0, justify='center', font='',  relief='raised', borderwidth=2, foreground='black', background='lightgrey', disabled=F, value=T, anchor='c', fill='none', bitmap='', group=parent )
	rtk->button( parent, text, type, padx, pady, width, height, justify, font, relief, borderwidth, foreground, background, disabled, value, anchor, fill, bitmap, group )

func scale ( parent, start=0.0, end=100.0, value=start, length=110, text='', resolution=1.0, orient='horizontal', width=15,
		 font='', relief='flat', borderwidth=2, foreground='black', background='lightgrey', fill='' )
	rtk->scale( parent, start, end, value, length, text, resolution, orient, width, font, relief, borderwidth, foreground, background, fill )

func text ( parent, width=30, height=8, wrap='word', font='', disabled=F, text='', relief='flat', borderwidth=2, foreground='black', background='lightgrey', fill='both' )
	rtk->text( parent, width, height, wrap, font, disabled, text, relief, borderwidth, foreground, background, fill )

func scrollbar ( parent, orient='vertical', width=15, foreground='black', background='lightgrey' )
	rtk->scrollbar( parent, orient, width, foreground, background )

func label ( parent, text='label', justify='left', padx=4, pady=2, font='', width=0, relief='flat', borderwidth=2, foreground='black', background='lightgrey', anchor='c', fill='none' )
	rtk->label( parent, text, justify, padx, pady, font, width, relief, borderwidth, foreground, background, anchor, fill )

func entry ( parent, width=30, justify='left', font='', relief='sunken', borderwidth=2, foreground='black', background='lightgrey', disabled=F, show=T, exportselection=T, fill='x' )
	rtk->entry( parent, width, justify, font, relief, borderwidth, foreground, background, disabled, show, exportselection, fill )

func message ( parent, text='message', width=180, justify='left', font='', padx=4, pady=2, relief='flat', borderwidth=3, foreground='black', background='lightgrey', anchor='c', fill='none' )
	rtk->message( parent, text, width, justify, font, padx, pady, relief, borderwidth, foreground, background, anchor, fill )

func listbox ( parent, width=20, height=6, mode='browse', font='', relief='sunken', borderwidth=2, foreground='black', background='lightgrey', exportselection=F, fill='x' )
	rtk->listbox( parent, width, height, mode, font, relief, borderwidth, foreground, background, exportselection, fill )

func canvas ( parent, width=200, height=150, region=[0,0,1000,400], relief='sunken', borderwidth=2, background='lightgrey', fill='both' )
	rtk->canvas( parent, width, height, region, relief, borderwidth, background, fill )

