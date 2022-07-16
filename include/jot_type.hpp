#pragma once

class JotType {

};

enum NumberKind {
    Integer1,
    Integer8,
    Integer16,
    Integer32,
    Integer64,
    Float32,
    Float64,
};

class JotNumber : public JotType {
public:
    JotNumber(NumberKind kind) : kind(kind) {}

    NumberKind get_kind() { return kind; }
private:
    NumberKind kind;
};

class JotNull : public JotType {

};

class JotPointer : public JotType {
public:
    JotPointer(JotType *point_to) : point_to(point_to) {}

    JotType* get_point_to() { return point_to; }
private:
    JotType *point_to;
};
