
func init_rtk( ) {

    #rtk := client('rtk',suspend=T)
    rtk := client('rtk')
    ret := [=]

    ret.frame := func ( parent=F, relief='flat', borderwidth=2, side='top', padx=0,
			pady=0, expand='both', background='lightgrey', width=70,
			height=50, cursor='', title='glish/tk', icon='', newcmap=F,
			tlead=F, tpos='sw' )
				rtk->frame( parent, relief, side, borderwidth, padx,
					    pady, expand, background, width, height,
					    cursor, title, icon, newcmap, tlead, tpos )

    ret.button := func ( parent, text='button', type='plain', padx=7, pady=3, width=0,
			 height=0, justify='center', font='',  relief='raised', borderwidth=2,
			 foreground='black', background='lightgrey', disabled=F, value=T,
			 anchor='c', fill='none', bitmap='', group=parent )
				rtk->button( parent, text, type, padx, pady, width, height,
					     justify, font, relief, borderwidth, foreground,
					     background, disabled, value, anchor, fill, bitmap,
					     group )

    ret.scale := func ( parent, start=0.0, end=100.0, value=start, length=110, text='',
			resolution=1.0,	orient='horizontal', width=15, font='', relief='flat',
			borderwidth=2, foreground='black', background='lightgrey', fill='' )
				rtk->scale( parent, start, end, value, length, text, resolution,
					    orient, width, font, relief, borderwidth, foreground,
					    background, fill )

    ret.text := func ( parent, width=30, height=8, wrap='word', font='', disabled=F, text='',
		       relief='flat', borderwidth=2, foreground='black', background='lightgrey',
		       fill='both' )
				rtk->text( parent, width, height, wrap, font, disabled, text,
					   relief, borderwidth, foreground, background, fill )

    ret.scrollbar := func ( parent, orient='vertical', width=15, foreground='black',
			    background='lightgrey' )
				rtk->scrollbar( parent, orient, width, foreground, background )

    ret.label := func ( parent, text='label', justify='left', padx=4, pady=2, font='', width=0,
			relief='flat', borderwidth=2, foreground='black', background='lightgrey',
			anchor='c', fill='none' )
				rtk->label( parent, text, justify, padx, pady, font, width, relief,
					    borderwidth, foreground, background, anchor, fill )

    ret.entry := func ( parent, width=30, justify='left', font='', relief='sunken', borderwidth=2,
		        foreground='black', background='lightgrey', disabled=F, show=T,
			exportselection=T, fill='x' )
				rtk->entry( parent, width, justify, font, relief, borderwidth,
					    foreground, background, disabled, show,
					    exportselection, fill )

    ret.message := func ( parent, text='message', width=180, justify='left', font='', padx=4,
			  pady=2, relief='flat', borderwidth=3, foreground='black',
			  background='lightgrey', anchor='c', fill='none' )
				rtk->message( parent, text, width, justify, font, padx, pady,
					      relief, borderwidth, foreground, background,
					      anchor, fill )

    ret.listbox := func ( parent, width=20, height=6, mode='browse', font='', relief='sunken',
			  borderwidth=2, foreground='black', background='lightgrey',
			  exportselection=F, fill='x' )
				rtk->listbox( parent, width, height, mode, font, relief,
					      borderwidth, foreground, background,
					      exportselection, fill )

    ret.canvas := func ( parent, width=200, height=150, region=[0,0,1000,400], relief='sunken',
			 borderwidth=2, background='lightgrey', fill='both' )
				rtk->canvas( parent, width, height, region, relief,
					     borderwidth, background, fill )

    ret.pgplot := func ( parent, width=200, height=150, region=[-100,100,-100,100], axis=-2,
			 nxsub=1, nysub=1, relief='sunken', borderwidth=2, padx=20, pady=20,
			 foreground='white', background='black', fill='both', mincolors=2,
			 maxcolors=100, cmapshare=F, cmapfail=F )
				rtk->pgplot( parent, width, height, region, axis, nxsub,
					     nysub, relief, borderwidth, padx, pady,
					     foreground, background, fill, mincolors,
					     maxcolors, cmapshare, cmapfail )
    return ref ret
}

dtk := init_rtk( )

frame := dtk.frame
button := dtk.button
scale := dtk.scale
text := dtk.text
scrollbar := dtk.scrollbar
label := dtk.label
entry := dtk.entry
message := dtk.message
listbox := dtk.listbox
canvas := dtk.canvas
pgplot := dtk.pgplot
