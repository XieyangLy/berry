#include "be_class.h"
#include "be_vector.h"
#include "be_map.h"
#include "be_string.h"
#include "be_gc.h"
#include "be_vm.h"
#include "be_func.h"
#include "be_var.h"

bclass* be_newclass(bvm *vm, bstring *name, bclass *super)
{
    bgcobject *gco = be_gcnew(vm, BE_CLASS, bclass);
    bclass *obj = cast_class(gco);
    if (obj) {
        obj->super = super;
        obj->fields = be_map_new(vm);
        obj->nvar = 0;
        obj->name = name;
        be_gc_fix(vm, gc_object(obj->fields));
        be_gc_fix(vm, gc_object(obj->name));
        be_gc_fix(vm, gc_object(obj));
    }
    return obj;
}

void be_field_bind(bclass *c, bstring *name)
{
    bmap *map = c->fields;
    bvalue *v = be_map_insertstr(map, name, NULL);
    v->v.i = c->nvar++;
    v->type = MT_VARIABLE;
}

void be_method_bind(bvm *vm, bclass *c, bstring *name, bproto *p)
{
    bvalue *m = be_map_insertstr(c->fields, name, NULL);
    bclosure *cl = be_newclosure(vm, 0);
    cl->proto = p;
    m->v.p = cl;
    m->type = MT_METHOD;
}

void be_prim_method_bind(bvm *vm, bclass *c, bstring *name, bcfunction f, int argc)
{
    bvalue *m = be_map_insertstr(c->fields, name, NULL);
    m->v.p = be_newntvfunc(vm, f, argc);
    m->type = MT_PRIMMETHOD;
}

bvalue* be_class_field(bclass *c, bstring *name)
{
    if (name) {
        return be_map_findstr(c->fields, name);
    }
    return NULL;
}

static binstance* newobject(bvm *vm, bclass *c)
{
    if (c) {
        size_t size = sizeof(binstance) + sizeof(bvalue) * (c->nvar - 1);
        bgcobject *gco = be_newgcobj(vm, BE_INSTANCE, size);
        binstance *obj = cast_instance(gco);
        if (obj) {
            obj->class = c;
            obj->super = newobject(vm, c->super);
            return obj;
        }
    }
    return NULL;
}

int be_class_newobj(bvm *vm, bclass *c, bvalue *argv, int argc)
{
    binstance *obj = newobject(vm, c);
    bmap *map = obj->class->fields;
    bvalue *init = be_map_findstr(map, be_newstr(vm, "init"));

    var_setinstance(argv, obj);
    if (init && init->type != MT_VARIABLE) { /* user constructor */
        bvalue *reg = argv + 1;
        /* copy argv */
        for (; argc > 0; --argc) {
            reg[argc] = argv[argc - 1];
        }
        *reg = *init; /* set constructor */
        return 1;
    }
    return 0;
}

bvalue* be_instance_field(binstance *obj, bstring *name, bvalue *dst)
{
    while (obj) {
        bvalue *m = be_class_field(obj->class, name);
        if (m) {
            if (m->type == MT_VARIABLE) {
                *dst = obj->fields[m->v.i];
            } else { /* method */
                *dst = *m;
            }
            return m;
        }
        obj = obj->super;
    }
    var_setnil(dst);
    return NULL;
}

int be_instance_setfield(binstance *obj, bstring *name, bvalue *src)
{
    while (obj) {
        bvalue *m = be_class_field(obj->class, name);
        if (m) {
            if (m->type == MT_VARIABLE) {
                obj->fields[m->v.i] = *src;
                return 1;
            }
            return 0;
        }
        obj = obj->super;
    }
    return 0;
}