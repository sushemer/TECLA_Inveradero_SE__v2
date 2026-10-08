CREATE DATABASE IF NOT EXISTS monitoreo_invernadero;

USE monitoreo_invernadero;


-- =====================================================
-- ESPECIFICACIONES DE UBICACIÓN
-- =====================================================

CREATE TABLE IF NOT EXISTS especificaciones (
    id INT AUTO_INCREMENT PRIMARY KEY,
    largo FLOAT COMMENT 'Distancia desde la entrada hacia adentro (metros)',
    ancho FLOAT COMMENT 'Distancia desde la izquierda hacia la derecha (metros)',
    altura FLOAT COMMENT 'Altura desde el suelo (metros)'
);


-- =====================================================
-- ZONAS
-- =====================================================

CREATE TABLE IF NOT EXISTS zona (
    id INT AUTO_INCREMENT PRIMARY KEY,
    zona VARCHAR(255)
);


-- =====================================================
-- TIPOS DE DISPOSITIVO
-- =====================================================

CREATE TABLE IF NOT EXISTS tipo_dispositivo (
    id INT AUTO_INCREMENT PRIMARY KEY,
    nombre VARCHAR(255),
    descripcion VARCHAR(255),
    image_path VARCHAR(255)
);


-- =====================================================
-- TIPOS DE DATOS
-- =====================================================

CREATE TABLE IF NOT EXISTS tipo_dato (
    id INT AUTO_INCREMENT PRIMARY KEY,
    nombre VARCHAR(255),
    unidad VARCHAR(100)
);


-- =====================================================
-- PROPÓSITOS
-- =====================================================

CREATE TABLE IF NOT EXISTS proposito (
    id INT AUTO_INCREMENT PRIMARY KEY,
    nombre VARCHAR(255) NOT NULL
);


-- =====================================================
-- DISPOSITIVOS
-- =====================================================

CREATE TABLE IF NOT EXISTS dispositivo (
    id INT AUTO_INCREMENT PRIMARY KEY,

    specs_id INT,
    zona_id INT NOT NULL,
    tipo_id INT NOT NULL,
    proposito_id INT NOT NULL,

    estado ENUM(
        'OPERATIVO',
        'INACTIVO',
        'EN MANTENIMIENTO',
        'FALLANDO'
    ),

    nombre VARCHAR(255),

    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,

    updated_at DATETIME
        DEFAULT CURRENT_TIMESTAMP
        ON UPDATE CURRENT_TIMESTAMP,

    CONSTRAINT fk_dispositivo_specs
        FOREIGN KEY (specs_id)
        REFERENCES especificaciones(id),

    CONSTRAINT fk_dispositivo_zona
        FOREIGN KEY (zona_id)
        REFERENCES zona(id),

    CONSTRAINT fk_dispositivo_tipo
        FOREIGN KEY (tipo_id)
        REFERENCES tipo_dispositivo(id),

    CONSTRAINT fk_dispositivo_proposito
        FOREIGN KEY (proposito_id)
        REFERENCES proposito(id)
);


-- =====================================================
-- LECTURAS
-- =====================================================

CREATE TABLE IF NOT EXISTS lecturas (
    id INT AUTO_INCREMENT PRIMARY KEY,

    dato FLOAT,

    dispositivo_id INT NOT NULL,

    tipo_dato_id INT NOT NULL,

    created_at DATETIME NOT NULL
        DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_lectura_dispositivo
        FOREIGN KEY (dispositivo_id)
        REFERENCES dispositivo(id),

    CONSTRAINT fk_lectura_tipo_dato
        FOREIGN KEY (tipo_dato_id)
        REFERENCES tipo_dato(id)
);