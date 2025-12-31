module.exports = grammar({
    name: 'fibril',

    rules: {
        source_file: $ => seq(
            $.syntax_declaration,
            $.package_declaration,
            repeat($.import_declaration),
            repeat(choice(
                $.struct_definition,
                $.node_definition
            ))
        ),

        // syntax = "fibril v2";
        syntax_declaration: $ => seq(
            'syntax',
            '=',
            $.string_literal,
            ';'
        ),

        // package robot.mobility;
        package_declaration: $ => seq(
            'package',
            $.qualified_identifier,
            ';'
        ),

        // import "common/types.fibril";
        import_declaration: $ => seq(
            'import',
            $.string_literal,
            ';'
        ),

        // struct Twist2D { ... }
        struct_definition: $ => seq(
            repeat($.attribute),
            'struct',
            $.identifier,
            '{',
            repeat($.field_declaration),
            '}'
        ),

        // node MobileBase { ... }
        node_definition: $ => seq(
            'node',
            $.identifier,
            '{',
            repeat($.port_declaration),
            '}'
        ),

        // float v;
        // Twist2D cmd;
        field_declaration: $ => seq(
            repeat($.attribute),
            $.type_spec,
            $.identifier,
            optional($.array_spec),
            optional(seq('=', $.literal)),
            ';'
        ),

        // sub Twist2D target_vel;
        // pub float voltage;
        // service enable_motor(Request) -> Response;
        port_declaration: $ => choice(
            $.sub_port,
            $.pub_port,
            $.service_port
        ),

        sub_port: $ => seq(
            repeat($.attribute),
            'sub',
            $.type_spec,
            $.identifier,
            ';'
        ),

        pub_port: $ => seq(
            repeat($.attribute),
            'pub',
            $.type_spec,
            $.identifier,
            ';'
        ),

        service_port: $ => seq(
            repeat($.attribute),
            'service',
            $.identifier,
            '(',
            optional($.type_spec),
            ')',
            '->',
            choice($.type_spec, 'void'),
            ';'
        ),

        // #[ros("/cmd_vel")]
        // #[ros_type(geometry_msgs/msg/Twist)]
        attribute: $ => seq(
            '#[',
            $.identifier,
            optional(seq(
                '(',
                commaSep1(choice($.string_literal, $.number_literal, $.qualified_identifier)),
                ')'
            )),
            ']'
        ),

        // 型指定
        type_spec: $ => choice(
            $.primitive_type,
            $.qualified_identifier
        ),

        primitive_type: $ => choice(
            'bool',
            'int8',
            'uint8',
            'int16',
            'uint16',
            'int32',
            'uint32',
            'int64',
            'uint64',
            'float',
            'double'
        ),

        // [10]
        array_spec: $ => seq(
            '[',
            $.number_literal,
            ']'
        ),

        // リテラル
        literal: $ => choice(
            $.number_literal,
            $.string_literal,
            $.bool_literal
        ),

        number_literal: $ => /[0-9]+(\.[0-9]+)?/,

        string_literal: $ => /"[^"]*"/,

        bool_literal: $ => choice('true', 'false'),

        // 識別子
        identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

        // package.name または geometry_msgs/msg/Twist
        qualified_identifier: $ => /[a-zA-Z_][a-zA-Z0-9_.\/]*/,

        // コメント
        comment: $ => token(choice(
            seq('//', /.*/),
            seq(
                '/*',
                /[^*]*\*+([^/*][^*]*\*+)*/,
                '/'
            )
        )),
    },

    extras: $ => [
        /\s/,
        $.comment
    ]
});

function commaSep1(rule) {
    return seq(rule, repeat(seq(',', rule)));
}
