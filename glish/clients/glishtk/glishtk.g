pragma include once

func init_glishtk( ) {
    global system

    gtk := client('glishtk')
#   gtk := client('glishtk', suspend=T)

    system.tk := gtk->version()

    ret := [=]

    ret.frame := func ( parent=F, relief='flat', borderwidth=2, side='top', padx=0,
			pady=0, expand='both', background='lightgrey', width=70,
			height=50, cursor='', title='glish/tk', icon='', newcmap=F,
			tlead=F, tpos='sw', hlcolor='', hlbackground='', hlthickness='',
			visual='', visualdepth=0, logfile='' )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->frame( parent, relief, side, borderwidth, padx,
						pady, expand, background, width, height,
						cursor, title, icon, newcmap, tlead, tpos,
						hlcolor, hlbackground, hlthickness, visual, visualdepth, logfile )
				}

    ret.button := func ( parent, text='button', type='plain', padx=7, pady=3, width=0,
			 height=0, justify='center', font='',  relief='raised', borderwidth=2,
			 foreground='black', background='lightgrey', disabled=F, value=T,
			 anchor='c', fill='none', bitmap='', group=parent,
			 hlcolor='', hlbackground='', hlthickness='' )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->button( parent, text, type, padx, pady, width, height,
						justify, font, relief, borderwidth, foreground,
						background, disabled, value, anchor, fill, bitmap,
						group, hlcolor, hlbackground, hlthickness )
				}

    ret.scale := func ( parent, start=0.0, end=100.0, value=start, length=110, text='',
			resolution=1.0,	orient='horizontal', width=15, font='', relief='flat',
			borderwidth=2, foreground='black', background='lightgrey', fill='',
			hlcolor='', hlbackground='', hlthickness='' )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->scale( parent, start, end, value, length, text, resolution,
						orient, width, font, relief, borderwidth, foreground,
						background, fill, hlcolor, hlbackground, hlthickness )
				}

    ret.text := func ( parent, width=30, height=8, wrap='word', font='', disabled=F, text='',
		       relief='flat', borderwidth=2, foreground='black', background='lightgrey',
		       fill='both', hlcolor='', hlbackground='', hlthickness='' )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->text( parent, width, height, wrap, font, disabled, text,
						relief, borderwidth, foreground, background, fill,
						hlcolor, hlbackground, hlthickness )
				}

    ret.scrollbar := func ( parent, orient='vertical', width=15, foreground='black',
			    background='lightgrey', jump=F, hlcolor='', hlbackground='', hlthickness=''  )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->scrollbar( parent, orient, width, foreground, background,
						jump, hlcolor, hlbackground, hlthickness )
				}

    ret.label := func ( parent, text='label', justify='left', padx=4, pady=2, font='', width=0,
			relief='flat', borderwidth=2, foreground='black', background='lightgrey',
			anchor='c', fill='none', hlcolor='', hlbackground='', hlthickness='' )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->label( parent, text, justify, padx, pady, font, width, relief,
						borderwidth, foreground, background, anchor, fill,
						hlcolor, hlbackground, hlthickness )
				}

    ret.entry := func ( parent, width=30, justify='left', font='', relief='sunken', borderwidth=2,
		        foreground='black', background='lightgrey', disabled=F, show=T,
			exportselection=T, fill='x', hlcolor='', hlbackground='', hlthickness='' )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->entry( parent, width, justify, font, relief, borderwidth,
						foreground, background, disabled, show, exportselection,
						fill, hlcolor, hlbackground, hlthickness )
				}

    ret.message := func ( parent, text='message', width=180, justify='left', font='', padx=4,
			  pady=2, relief='flat', borderwidth=3, foreground='black',
			  background='lightgrey', anchor='c', fill='none',
			  hlcolor='', hlbackground='', hlthickness='' )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->message( parent, text, width, justify, font, padx, pady,
						relief, borderwidth, foreground, background,
						anchor, fill, hlcolor, hlbackground, hlthickness )
				}

    ret.listbox := func ( parent, width=20, height=6, mode='browse', font='', relief='sunken',
			  borderwidth=2, foreground='black', background='lightgrey',
			  exportselection=F, fill='x', hlcolor='', hlbackground='', hlthickness='' )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->listbox( parent, width, height, mode, font, relief,
						borderwidth, foreground, background, exportselection,
						fill, hlcolor, hlbackground, hlthickness )
				}

    ret.canvas := func ( parent, width=200, height=150, region=[0,0,1000,400], relief='sunken',
			 borderwidth=2, background='lightgrey', fill='both', hlcolor='',
			 hlbackground='', hlthickness='' )
				{
				if ( system.nogui ) fail "GUI disabled"
				return gtk->canvas( parent, width, height, region, relief, borderwidth,
						background, fill, hlcolor, hlbackground, hlthickness )
				}

    ret.have_gui := func ( ) { return ! system.nogui && gtk->have_gui() }
    ret.tk_hold := func ( ) { if ( ! system.nogui ) gtk->tk_hold(T); return T }
    ret.tk_release := func ( ) { if ( ! system.nogui ) gtk->tk_release(T); return T }
    ret.tk_iconpath := func ( path ) { gtk->tk_iconpath(path); return T }
    ret.tk_checkcolor := func ( color ) { return ! system.nogui && gtk->tk_checkcolor( color ) }

    ret.tk_load := func ( module_name, init_func, needtk=T ) {
			if ( is_function(init_func) && is_string(module_name) ) {
			    if ( is_fail(gtk->tk_load( module_name, needtk=needtk )) ) fail
			    return init_func( gtk, ret )
			}
			fail 'bad parameter' }

    ret.tk_loadpath := func ( path ) { gtk->tk_loadpath( path ); return T }

    ret.tk_loadpath( system.path.lib[system.host] )

    return ref ret
}

dgtk := init_glishtk( )

frame := dgtk.frame
button := dgtk.button
scale := dgtk.scale
text := dgtk.text
scrollbar := dgtk.scrollbar
label := dgtk.label
entry := dgtk.entry
message := dgtk.message
listbox := dgtk.listbox
canvas := dgtk.canvas

have_gui := dgtk.have_gui
tk_hold := dgtk.tk_hold
tk_release := dgtk.tk_release
tk_iconpath := dgtk.tk_iconpath
tk_checkcolor := dgtk.tk_checkcolor
tk_load := dgtk.tk_load
tk_loadpath := dgtk.tk_loadpath
