    f := frame()
    cf := frame(f,side='left',borderwidth=0)
    c := canvas(cf)
    vsb := scrollbar(cf)

    bf := frame(f,side='right',borderwidth=0,expand='x')
    pad := frame(bf,expand='none',width=23,height=23,relief='groove')
    hsb := scrollbar(bf,orient='horizontal')

    whenever vsb->scroll, hsb->scroll do
        c->view($value)
    whenever c->yscroll do
        vsb->view($value)
    whenever c->xscroll do
        hsb->view($value)
    poly := c->poly(20,-40,40,-20,40,20,20,40,-20,40,-40,
                    20,-40,-20,-20,-40,fill='red',tag='stop')
    edge := c->line(20,-40,40,-20,40,20,20,40,-20,40,-40,20,-40,-20,
                    -20,-40,20,-40,fill='white',width='5',tag='stop')
    word := c->text(0,0,text='STOP',fill='white',tag='stop')
    c->move("all",50,50)
    c->bind('stop','<Button-1>','snag')
    c->bind('stop','<B1-Motion>','drag')
    whenever c->snag do
        state := $value.device
    whenever c->drag do
        {
        tmp := c->move($value.tag, $value.device - state)
        state := $value.device
        }
